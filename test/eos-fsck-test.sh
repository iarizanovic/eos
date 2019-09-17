#!/bin/bash -e

# ******************************************************************************
# EOS - the CERN Disk Storage System
# Copyright (C) 2019 CERN/Switzerland
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# ******************************************************************************

function usage() {
  echo "usage: $(basename $0) --type docker/k8s <k8s_namespace>"
  echo "       docker : script runs in a Docker based setup"
  echo "       k8s    : script runs in a Kubernetes setup and requires a namespace argument"
}

# Get name of the Pod given the "namespace" and the "label" paramenters.
# $1 : Kubernetes namespace
# $2 : label selector specifying attributes for a Kubernetes object
## Example of admitted labels are {eos-mgm1, eos-mq, eos-fst1, eos-fst2 ... }, mirroring eos-roles
# refs :https://kubernetes.io/docs/concepts/overview/working-with-objects/labels/
function get_podname() {
  local NAMESPACE=$1
  local LABEL=$2

  if [[ -z "${NAMESPACE}" || -z "${LABEL}" ]]; then
    echo "error: get_podname called with invalid/empty arguments"
    exit 1
  fi

  kubectl get pods --namespace=${NAMESPACE} --no-headers -o custom-columns=":metadata.name" -l app=${LABEL}
}

# Forward the given command to the proper executor Docker or Kubernetes. Gets
# as argument a container name and a shell command to be executed
function exec_cmd() {
  if [[ "${IS_DOCKER}" == true ]]; then
    exec_cmd_docker "$@"
  else
    exec_cmd_k8s "$@"
  fi
}

# Execute command in Docker setup where the first argument is the name of the container
# and the rest is the command to be executed
function exec_cmd_docker() {
  set -o xtrace
  docker exec -i $1 /bin/bash -l -c "${@:2}"
  set +o xtrace
}

# Create and upload test files to the eos instance. We create a random file and
# upload it multiple times to EOS which one file per type of corruption.
function create_test_files() {
  EOS_ROOT=/eos/dockertest

  exec_cmd eos-client1-test "dd if=/dev/urandom of=/tmp/test_file.dat bs=1M count=10"
  exec_cmd eos-client1-test "eos -r 0 0 mkdir ${EOS_ROOT}/fsck; eos -r 0 0 attr set default=replica ${EOS_ROOT}/fsck"
  # Create one file per type of fsck error and trip whitespaces
  exec_cmd eos-client1-test "xrdcp -f -d 1 /tmp/test_file.dat \${EOS_MGM_URL}/${EOS_ROOT}/fsck/file_d_mem_sz_diff.dat"
  FXID_D_MEM_SZ_DIFF=$(exec_cmd eos-client1-test "eos fileinfo ${EOS_ROOT}/fsck/file_d_mem_sz_diff.dat -m | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '\$1==\"fxid\" {print \$2};'")
  FXID_D_MEM_SZ_DIFF=$(echo "${FXID_D_MEM_SZ_DIFF}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  exec_cmd eos-client1-test "xrdcp -f -d 1 /tmp/test_file.dat \${EOS_MGM_URL}/${EOS_ROOT}/fsck/file_m_mem_sz_diff.dat"
  FXID_M_MEM_SZ_DIFF=$(exec_cmd eos-client1-test "eos fileinfo ${EOS_ROOT}/fsck/file_m_mem_sz_diff.dat -m | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '\$1==\"fxid\" {print \$2};'")
  FXID_M_MEM_SZ_DIFF=$(echo "${FXID_M_MEM_SZ_DIFF}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  exec_cmd eos-client1-test "xrdcp -f -d 1 /tmp/test_file.dat \${EOS_MGM_URL}/${EOS_ROOT}/fsck/file_d_cx_diff.dat"
  FXID_D_CX_DIFF=$(exec_cmd eos-client1-test "eos fileinfo ${EOS_ROOT}/fsck/file_d_cx_diff.dat -m | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '\$1==\"fxid\" {print \$2};'")
  FXID_D_CX_DIFF=$(echo "${FXID_D_CX_DIFF}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  exec_cmd eos-client1-test "xrdcp -f -d 1 /tmp/test_file.dat \${EOS_MGM_URL}/${EOS_ROOT}/fsck/file_m_cx_diff.dat"
  FXID_M_CX_DIFF=$(exec_cmd eos-client1-test "eos fileinfo ${EOS_ROOT}/fsck/file_m_cx_diff.dat -m | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '\$1==\"fxid\" {print \$2};'")
  FXID_M_CX_DIFF=$(echo "${FXID_M_CX_DIFF}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  exec_cmd eos-client1-test "xrdcp -f -d 1 /tmp/test_file.dat \${EOS_MGM_URL}/${EOS_ROOT}/fsck/file_unreg.dat"
  FXID_UNREG=$(exec_cmd eos-client1-test "eos fileinfo ${EOS_ROOT}/fsck/file_unreg.dat -m | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '\$1==\"fxid\" {print \$2};'")
  FXID_UNREG=$(echo "${FXID_UNREG}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  exec_cmd eos-client1-test "xrdcp -f -d 1 /tmp/test_file.dat \${EOS_MGM_URL}/${EOS_ROOT}/fsck/file_rep_missing.dat"
  FXID_REP_MISSING=$(exec_cmd eos-client1-test "eos fileinfo ${EOS_ROOT}/fsck/file_rep_missing.dat -m | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '\$1==\"fxid\" {print \$2};'")
  FXID_REP_MISSING=$(echo "${FXID_REP_MISSING}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  exec_cmd eos-client1-test "xrdcp -f -d 1 /tmp/test_file.dat \${EOS_MGM_URL}/${EOS_ROOT}/fsck/file_rep_diff_under.dat"
  FXID_REP_DIFF_UNDER=$(exec_cmd eos-client1-test "eos fileinfo ${EOS_ROOT}/fsck/file_rep_diff_under.dat -m | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '\$1==\"fxid\" {print \$2};'")
  FXID_REP_DIFF_UNDER=$(echo "${FXID_REP_DIFF_UNDER}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  exec_cmd eos-client1-test "xrdcp -f -d 1 /tmp/test_file.dat \${EOS_MGM_URL}/${EOS_ROOT}/fsck/file_rep_diff_over.dat"
  FXID_REP_DIFF_OVER=$(exec_cmd eos-client1-test "eos fileinfo ${EOS_ROOT}/fsck/file_rep_diff_over.dat -m | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '\$1==\"fxid\" {print \$2};'")
  FXID_REP_DIFF_OVER=$(echo "${FXID_REP_DIFF_OVER}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  exec_cmd eos-client1-test "xrdcp -f -d 1 /tmp/test_file.dat \${EOS_MGM_URL}/${EOS_ROOT}/fsck/file_orphan.dat"
  FXID_ORPHAN=$(exec_cmd eos-client1-test "eos fileinfo ${EOS_ROOT}/fsck/file_orphan.dat -m | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '\$1==\"fxid\" {print \$2};'")
  FXID_ORPHAN=$(echo "${FXID_ORPHAN}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')

  # If any of the FXID_* variables are empty then we have a problem
  if [[ -z "${FXID_D_MEM_SZ_DIFF}"  ||
        -z "${FXID_M_MEM_SZ_DIFF}"  ||
        -z "${FXID_D_CX_DIFF}"      ||
        -z "${FXID_M_CX_DIFF}"      ||
        -z "${FXID_UNREG}"          ||
        -z "${FXID_REP_MISSING}"    ||
        -z "${FXID_REP_DIFF_UNDER}" ||
        -z "${FXID_REP_DIFF_OVER}"  ||
        -z "${FXID_ORPHAN}" ]]; then
    echo "error: some of the fxids could not be retrieved"
    exit 1
  fi

  # Cleanup generated test file
  exec_cmd eos-client1-test "rm -rf /tmp/test_file.dat"
}

# Corrupt file to generate d_mem_sz_diff error
function corrupt_d_mem_sz_diff() {
  local CMD_OUT=$(exec_cmd eos-client1-test "eos fileinfo fxid:${FXID_D_MEM_SZ_DIFF} -m --fullpath | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '{if (\$1 ==\"fsid\" || \$1 ==\"fullpath\") {print \$2};}' | tail -n2")
  # Extract the fxid and local path, trim the input
  local FSID=$(echo "${CMD_OUT}" | head -n1 | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  local LPATH=$(echo "${CMD_OUT}" | tail -n1)
  echo "Fsid: ${FSID}, local_path=${LPATH}"
  exec_cmd "eos-fst${FSID}-test" "echo \"dummy\" >> ${LPATH}"
}

# Corrupt file to generate m_mem_sz_diff
function corrupt_m_mem_sz_diff() {
  # Use the eos-ns-inspect tool to corrupt the MGM file size
  :
}

# Corrupt file to generate d_cx_diff error
function corrupt_d_cx_diff() {
  local CMD_OUT=$(exec_cmd eos-client1-test "eos fileinfo fxid:${FXID_D_CX_DIFF} -m --fullpath | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '{if (\$1 ==\"fsid\" || \$1 ==\"fullpath\") {print \$2};}' | tail -n2")
  # Extract the fxid and local path, trim the input
  local FSID=$(echo "${CMD_OUT}" | head -n1 | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  local LPATH=$(echo "${CMD_OUT}" | tail -n1)
  # Corrupt the checkusm of the file by writing random bytes to it
  exec_cmd "eos-fst${FSID}-test" "dd if=/dev/urandom of=${LPATH} bs=1M count=10"
}

# Corrupt file to generate m_cx_diff
function corrupt_m_cx_diff() {
  # Use the eos-ns-inspect tool to corrupt the MGM checksum value
  :
}

# Corrupt file to generate rep_missing_n error
function corrupt_rep_missing_n {
 local CMD_OUT=$(exec_cmd eos-client1-test "eos fileinfo fxid:${FXID_REP_MISSING} -m --fullpath | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '{if (\$1 ==\"fsid\" || \$1 ==\"fullpath\") {print \$2};}' | tail -n2")
  # Extract the fxid and local path, trim the input
  local FSID=$(echo "${CMD_OUT}" | head -n1 | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  local LPATH=$(echo "${CMD_OUT}" | tail -n1)
  exec_cmd "eos-fst${FSID}-test" "rm -rf ${LPATH}"
}

# Corrupt file to generate rep_diff_under error
function corrupt_rep_diff_under() {
  local CMD_OUT=$(exec_cmd eos-client1-test "eos fileinfo fxid:${FXID_REP_DIFF_UNDER} -m --fullpath | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '{if (\$1 ==\"fsid\" || \$1 ==\"fullpath\") {print \$2};}' | tail -n2")
  # Extract the fxid and local path, trim the input
  local FSID=$(echo "${CMD_OUT}" | head -n1 | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  local LPATH=$(echo "${CMD_OUT}" | tail -n1)
  exec_cmd eos-client1-test "eos -r 0 0 file drop fxid:${FXID_REP_DIFF_UNDER} ${FSID}"
}

# Corrupt file to generate rep_diff_over error
function corrupt_rep_diff_over() {
  local CMD_OUT=$(exec_cmd eos-client1-test "eos fileinfo fxid:${FXID_REP_DIFF_OVER} -m --fullpath | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '{if (\$1 ==\"fsid\") {print \$2};}' | tail -n2")
  # Extract the fxid and local path, trim the input
  local VECT_FSID=( $(echo "${CMD_OUT}" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//') )
  echo "Used locations: ${VECT_FSID[@]}"
  local NEW_FSID=""

  for i in {1..7}; do
    local found=false
    for e in ${VECT_FSID[@]}; do
      if [[ "$i" == "$e" ]]; then
        found=true
        break
      fi
    done
    if [[ "${found}" == false ]]; then
      NEW_FSID=$i
      break
    fi
  done

  if [[ "${NEW_FSID}" == "" ]]; then
    echo "error: no new FSID found for replication command"
    exit 1
  fi

  exec_cmd eos-client1-test "eos -r 0 0 file replicate fxid:${FXID_REP_DIFF_OVER} ${VECT_FSID[0]} ${NEW_FSID}"
}

# Corrupt file to generate file_unreg error
function corrupt_unreg() {
  local CMD_OUT=$(exec_cmd eos-client1-test "eos fileinfo fxid:${FXID_UNREG} -m --fullpath | sed -r 's/[[:alnum:]]+=/\n&/g' | awk -F '=' '{if (\$1 ==\"fsid\" || \$1 ==\"fullpath\") {print \$2};}' | tail -n2")
  # Extract the fxid and local path, trim the input
  local FSID=$(echo "${CMD_OUT}" | head -n1 | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
  exec_cmd eos-client1-test "eos -r 0 0 file drop fxid:${FXID_UNREG} ${FSID} -f"
}

# Configure fsck to run more often and reduce the scan times
function configure_fsck() {
  # First reduce the scan interval on the FSTs
  for i in {1..7}; do
    exec_cmd eos-client1-test  "eos -r 0 0 fs config ${i} scan_disk_interval=100;\
                                eos -r 0 0 fs config ${i} scan_ns_interval=100;\
                                eos -r 0 0 fs config ${i} scaninterval=100"
  done

  # Reduce the interval when the fsck collection thread runs
  exec_cmd eos-client1-test  "eos -r 0 0 fsck enable 1;"
}

# Check that we collected all the errors that we expect
function check_all_errors_collected() {
  # Allow for at most MAX_DELAY seconds to collect all the errors
  local MAX_DELAY=120
  local START_TIME=$(date +%s)
  while
    local CURRENT_TIME=$(date +%s)
    local FOUND_D_MEM_SZ_DIFF=$(exec_cmd eos-client1-test "eos -r 0 0 fsck report -i | grep ${FXID_D_MEM_SZ_DIFF}")
    local FOUND_M_MEM_SZ_DIFF=$(exec_cmd eos-client1-test "eos -r 0 0 fsck report -i | grep ${FXID_M_MEM_SZ_DIFF}")
    local FOUND_D_CX_DIFF=$(exec_cmd eos-client1-test "eos -r 0 0 fsck report -i | grep ${FXID_D_CX_DIFF}")
    # @todo(esindril) local FOUND_M_CX_DIFF=$(exec_cmd eos-client1-test "eos -r 0 0 fsck report -i | grep ${FXID_M_CX_DIFF}")
    local FOUND_UNREG=$(exec_cmd eos-client1-test "eos -r 0 0 fsck report -i | grep ${FXID_UNREG}")
    local FOUND_REP_MISSING=$(exec_cmd eos-client1-test "eos -r 0 0 fsck report -i | grep ${FXID_REP_MISSING}")
    local FOUND_REP_DIFF_UNDER=$(exec_cmd eos-client1-test "eos -r 0 0 fsck report -i | grep ${FXID_REP_DIFF_UNDER}")
    local FOUND_REP_DIFF_OVER=$(exec_cmd eos-client1-test "eos -r 0 0 fsck report -i | grep ${FXID_REP_DIFF_OVER}")
    # @todo(esindril) local FOUND_ORPHAN=$(exec_cmd eos-client1-test "eos -r 0 0 fsck report -i | grep ${FXID_ORPHAN}")

    if [[ -z "${FOUND_D_MEM_SZ_DIFF}"  ||
#          -z "${FOUND_M_MEM_SZ_DIFF}"  ||
          -z "${FOUND_D_CX_DIFF}"      ||
          -z "${FOUND_UNREG}"          ||
          -z "${FOUND_REP_MISSING}"    ||
          -z "${FOUND_REP_DIFF_UNDER}" ||
          -z "${FOUND_REP_DIFF_OVER}" ]]; then
      if (( $((${CURRENT_TIME} - ${START_TIME})) >= ${MAX_DELAY} )); then
        echo "error: some of the errors were not discovered"
        exit 1
      else
        echo "info: sleep for 5 seconds waiting for error collection"
        sleep 5
      fi

      (( $((${CURRENT_TIME} - ${START_TIME})) < ${MAX_DELAY} ))
    else
     echo "info: found all the errors we were expecting"
     false # to end the loop
    fi
  do
   :
  done
}

# Set up global variables
IS_DOCKER=false
NAMESPACE=""

if [[ $# -lt 2 ]]; then
  echo "error: invalid number of arguments"
  usage
  exit 1
fi

if [[ "$1" != "--type" ]]; then
  echo "error: unknown argument \"$1\""
  usage
  exit 1
fi

if [[ "$2" == "docker" ]]; then
  IS_DOCKER=true
elif [[ "$2" == "k8s" ]]; then
  IS_DOCKER=false
else
  echo "error: unknown type of executor \"$2\""
  usage
  exit 1
fi

if [[ "${IS_DOCKER}" == false ]]; then
  # For the Kubernetes setup we also need a namespace argument
  if [[ $# -le 3 ]]; then
    echo "error: missing Kubernetes namespace argument"
    usage
    exit 1
  fi

  NAMESPACE="$3"
fi

# Create test file
create_test_files

# Create different type of corruptions for different files
corrupt_d_mem_sz_diff
# corrupt_m_mem_sz_diff
corrupt_d_cx_diff
corrupt_m_cx_diff
corrupt_rep_missing_n
corrupt_rep_diff_under
corrupt_rep_diff_over
corrupt_unreg

# Configure fsck to run more often and reduce scan times
configure_fsck

# Check that we are collecting all the expected errors
check_all_errors_collected

# Enable the repair thread and allow a delay of 2 minutes to
# correct all the discovered errors
exec_cmd eos-client1-test "eos -r 0 0 fsck config toggle-repair"

sleep 100

exec_cmd eos-client1-test "eos -r 0 0 fsck report | grep . && echo 1 || echo 0"

echo "Return value is: $?"

# Clean up EOS space
exec_cmd eos-client1-test "eos rm -rF ${EOS_ROOT}/fsck
