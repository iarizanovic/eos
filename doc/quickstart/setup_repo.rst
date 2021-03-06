.. index::
   single: Setup yum repositories

.. _eos_base_setup_repos:

Setup YUM Repository
====================

EOS Beryl-Aquamarine
-------------------------------------------

EOS Repo (eventually change el-7 to your platform: el-5 el-6 el-7 fc-21 fc-22 fc-23 fc-rawhide). Create file /etc/yum.repos.d/eos.repo with following content

.. code-block:: text

   [eos-aquamarine]
   name=EOS 0.3 Version
   baseurl=https://dss-ci-repo.web.cern.ch/dss-ci-repo/eos/aquamarine/tag/el-7/x86_64/
   gpgcheck=0

Create file /etc/yum.repos.d/eos-dep.repo with following content

.. code-block:: text

   [eos-dep]
   name=EOS 0.3 Dependencies
   baseurl=https://dss-ci-repo.web.cern.ch/dss-ci-repo/eos/aquamarine-depend/el-7-x86_64/
   gpgcheck=0

.. warning::
   You have to add line "exclude=xrootd*,libmicrohttp*" in epel.repo and epel-testing.repo.repo . This will be removed in future.



EOS Citrine
-------------------------------------------

EOS Repo (eventually change el-7 to your platform: el-5 el-6 el-7 fc-21 fc-22 fc-23 fc-rawhide). Create file /etc/yum.repos.d/eos.repo with following content

.. code-block:: text

   [eos-citrine]
   name=EOS 4.0 Version
   baseurl=https://dss-ci-repo.web.cern.ch/dss-ci-repo/eos/citrine/tag/el-7/x86_64/
   gpgcheck=0
