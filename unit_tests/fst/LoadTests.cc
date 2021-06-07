//------------------------------------------------------------------------------
// File: LoadTests.cc
// Author: Elvin Sindrilaru - CERN
//------------------------------------------------------------------------------

/************************************************************************
 * EOS - the CERN Disk Storage System                                   *
 * Copyright (C) 2019 CERN/Switzerland                                  *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
 ************************************************************************/

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#define IN_TEST_HARNESS
#include "fst/Load.hh"
#undef IN_TEST_HARNESS
#include <fstream>
#include <cstdio>

namespace
{
void CreateCentOS7InputFile(const std::string& path)
{
  std::string test_input {"  70       0 sdcs 124571 170 7182956 42686 22398716 9146198 607011590 8079794 0 1871023 8112828\n"
                          "  70       1 sdcs1 382 110 29037 172 339 152 7750 1180 0 415 1352\n"
                          "  70       2 sdcs2 478 0 30970 191 1622 812 273656 7489 0 1802 7675\n"
                          "  70       3 sdcs3 117464 59 6292546 35156 225861 1055753 23811784 829923 0 51053 865280\n"
                          "  70       4 sdcs4 479 0 8634 92 8517693 7663207 135817760 385225 0 262544 380895\n"
                          "  70       5 sdcs5 5617 1 818433 7056 9342414 426274 447100640 6491379 0 1285415 6493505\n"
                          "  70      16 sdct 51537 0 2853072 20365 0 0 0 0 0 8677 20337\n"
                          "   8       0 sda 24898 0 24000401 400573 43640 260124 15018512 651076 0 240307 1051343\n"
                          "   8       1 sda1 24668 0 23984057 399775 41573 8 12921152 56148 0 235511 455617\n"
                          "   8      16 sdb 24973 0 24009513 388694 43706 260128 15017720 646223 0 227788 1034598\n"
                          "   8      17 sdb1 24669 0 23986289 387029 41639 12 12920360 54514 0 222660 441224\n"
                          "   8      64 sde 24930 0 23999057 417676 43647 260127 15017128 651445 0 240014 1068802\n"
                          "   8      65 sde1 24655 0 23978681 415947 41580 11 12919768 63516 0 234896 479144\n"
                          "   8      32 sdc 26119 0 25056833 459243 43702 260127 15017672 640526 0 250839 1099511\n"
                          "   8      33 sdc1 25739 0 25024497 457203 41635 11 12920312 54773 0 245605 511720\n"
                          "   8     176 sdl 26168 0 25054537 427576 43671 260124 15017456 647736 0 244483 1074988\n"
                          "   8     177 sdl1 25816 0 25030825 426352 41604 8 12920096 64060 0 239437 490089\n"
                          "   8     112 sdh 25063 0 24005161 454962 43708 260124 15017816 664354 0 259738 1119001\n"
                          "   8     113 sdh1 24704 0 23981473 453574 41641 8 12920456 63896 0 254530 517157\n"
                          "  65     128 sdy 25086 0 24010561 397401 43657 260124 15015312 636254 0 220940 1033312\n"
                          "  65     129 sdy1 24649 0 23973129 395836 41590 8 12917952 51106 0 215757 446600\n"
                          "   8      48 sdd 25009 0 24003273 419821 43665 260127 15017440 648834 0 234443 1068330\n"
                          "   8      49 sdd1 24626 0 23975153 417569 41598 11 12920080 56601 0 229143 473847\n"
                          "  67      64 sdba 26110 0 25046537 418532 43654 260127 15017296 637665 0 223418 1055890\n"
                          "  67      65 sdba1 25798 0 25028737 417131 41587 11 12919936 48074 0 218215 464898\n"
                          "  67      96 sdbc 25046 0 24005097 389485 43671 260124 15017456 630291 0 210333 1019466\n"
                          "  67      97 sdbc1 24687 0 23981409 388145 41604 8 12920096 48373 0 205315 436210\n"
                          "   8      80 sdf 26055 0 25047033 439238 43649 260127 15017144 647190 0 240421 1086108\n"
                          "   8      81 sdf1 25751 0 25028305 437977 41582 11 12919784 55055 0 235301 492716\n"
                          "  67     112 sdbd 25004 0 24002121 392144 43646 260124 15017032 638482 0 212397 1030326\n"
                          "  67     113 sdbd1 24689 0 23979369 390699 41579 8 12919672 49581 0 207233 439982\n"
                          "   8     128 sdi 26157 0 25061441 412592 43687 260124 15017640 655536 0 227463 1067810\n"
                          "   8     129 sdi1 25812 0 25036313 411120 41620 8 12920280 55887 0 222252 466689\n"
                          "  67      32 sday 24991 0 24000977 439389 43685 260127 15017488 632277 0 237513 1071356\n"
                          "  67      33 sday1 24683 0 23982065 438059 41618 11 12920128 49938 0 232457 487689\n"
                          "  65      32 sds 24929 0 23999041 404269 43698 260123 15017776 632673 0 219102 1036640\n"
                          "  65      33 sds1 24654 0 23978665 402404 41631 7 12920416 48588 0 213945 450691\n"
                          "   8      96 sdg 25008 0 24002649 588711 43662 260124 15017328 663959 0 320216 1252336\n"
                          "   8      97 sdg1 24699 0 23980737 587187 41595 8 12919968 58339 0 315018 645194\n"
                          "  67      16 sdax 26175 0 25055665 402542 43700 260127 15017552 644708 0 222536 1046921\n"
                          "  67      17 sdax1 25816 0 25030825 401179 41633 11 12920192 56272 0 217401 457122\n"
                          "   8     240 sdp 25025 0 24006121 407314 43658 260124 15017352 647138 0 221652 1054107\n"
                          "   8     241 sdp1 24675 0 23981057 405845 41591 8 12919992 49647 0 216445 455149\n"
                          "   8     160 sdk 25039 0 24015121 421186 43652 260127 15017384 664389 0 235533 1085207\n"
                          "   8     161 sdk1 24636 0 23975137 419088 41585 11 12920024 57512 0 230137 476235"};
  std::ofstream ostream(path);
  ostream << test_input;
}

void CreateCentOS8InputFile(const std::string& path)
{
  std::string test_input {"   8       0 sda 62814337 524435 4646361308 90413178 210006603 133937217 4752049454 123891868 0 88396457 144498889 0 0 0 0\n"
                          "   8       1 sda1 44357 160 3875270 20003 582 33 8791 5948 0 19549 13843 0 0 0 0\n"
                          "   8       2 sda2 55651 98 10212258 74098 3545 1567 822360 25536 0 40697 80694 0 0 0 0\n"
                          "   8       3 sda3 21694274 458256 1383181930 22325828 1009165 3572894 108689320 3562076 0 8763302 19768104 0 0 0 0\n"
                          "   8       4 sda4 2633369 2408 23361866 869268 144471520 126339714 2185019688 52003183 0 49234388 17950954 0 0 0 0\n"
                          "   8       5 sda5 38382439 63513 3225666938 67121603 54725758 4023009 2457509295 67917046 0 38529063 106676223 0 0 0 0\n"
                          "   8      16 sdb 280004 0 12825118 101274 0 0 0 0 0 63540 17551 0 0 0 0\n"
                          "  65     240 sdaf 4835915 71763 5288681580 213862896 11211921 357170 10782583736 2916512244 0 54571161 3122380621 0 0 0 0\n"
                          "  65     241 sdaf1 4665421 71763 5288491985 213657950 11211893 357170 10782583736 2916512128 0 54348315 3122281218 0 0 0 0\n"
                          "  67       0 sdaw 5132690 72110 5664992628 229793587 11628206 319287 11219297672 3239071197 0 58688049 3460542577 0 0 0 0\n"
                          "  67       1 sdaw1 4962120 72110 5664802969 229590667 11628185 319287 11219297672 3239071093 0 58468389 3460444505 0 0 0 0\n"
                          "  65     208 sdad 26117135 74997 28147876639 1065616901 57907254 2875406 59112697693 2610814266 0 260350218 3634551765 0 0 0 0\n"
                          "  65     209 sdad1 25946626 74997 28147687156 1065340405 57850821 2875406 59112697693 2610111396 3 259511576 3633701039 0 0 0 0\n"
                          "   8     208 sdn 17334830 70516 18458122564 560353447 48941970 2436967 49802632536 2823531355 0 193566784 3350896123 0 0 0 0\n"
                          "   8     209 sdn1 17164413 70516 18457933017 560106935 48939128 2436967 49802632536 2823452930 0 193329473 3350676821 0 0 0 0\n"
                          "   8      32 sdc 22108273 66528 23280243569 743613786 53533892 2717832 54317033842 2541659795 0 219933553 3247514864 0 0 0 0\n"
                          "   8      33 sdc1 21937698 66528 23280054081 743332159 53529704 2717832 54317033842 2541586060 0 219693757 3247266128 0 0 0 0\n"
                          "  66     160 sdaq 8094319 81223 10852891035 461439208 15135086 447325 16835996519 3361101232 0 102462698 3811146326 0 0 0 0\n"
                          "  66     161 sdaq1 7923786 81223 10852701472 461195871 15084443 447325 16835996519 3360501796 0 101676359 3810430379 0 0 0 0\n"
                          "  66      96 sdam 4817434 62698 5295964796 194913778 11357679 318771 10952944704 3060699399 0 56002536 3247589757 0 0 0 0\n"
                          "  66      97 sdam1 4646991 62698 5295775441 194710559 11357654 318771 10952944704 3060699092 0 55784529 3247491599 0 0 0 0\n"
                          "  68      48 sdbp 4911712 73960 5311779060 214483981 12516969 325938 12150744688 3470401361 0 58675202 3676212304 0 0 0 0\n"
                          "  68      49 sdbp1 4741209 73960 5311589897 214278578 12516939 325938 12150744688 3470400970 0 58458296 3676111101 0 0 0 0\n"
                          "  66       0 sdag 4835854 66406 5314653772 190893832 11240706 318482 10844132432 2923637639 0 54370426 3106530380 0 0 0 0\n"
                          "  66       1 sdag1 4665378 66406 5314464553 190691599 11240683 318482 10844132432 2923637381 0 54150225 3106432951 0 0 0 0\n"
                          "  65       0 sdq 26246305 54194 28111458331 808617361 52603906 3000779 53274265737 2100604593 0 227312235 2869914567 0 0 0 0\n"
                          "  65       1 sdq1 26076215 54194 28111269064 808357702 52600398 3000779 53274265737 2100559853 0 227071761 2869715833 0 0 0 0\n"
                          "  66     128 sdao 4550123 59510 4933290700 156350825 11537898 333383 11097470448 2997782846 0 53945735 3146164821 0 0 0 0\n"
                          "  66     129 sdao1 4379645 59510 4933101481 156151412 11537878 333383 11097470448 2997782705 0 53723385 3146070643 0 0 0 0\n"
                          "  68     144 sdbv 5164776 73942 5615039540 218744601 12755370 321120 12349366672 3637308163 0 60468784 3847162423 0 0 0 0"};
  std::ofstream ostream(path);
  ostream << test_input;
}
}

TEST(Load, DiskStats)
{
  eos::fst::DiskStat ds_unkown;
  ASSERT_FALSE(ds_unkown.Measure("/unkown/path"));
  // Check CentOS7 file format
  std::string tmp_path_c7 = std::tmpnam(nullptr);
  CreateCentOS7InputFile(tmp_path_c7);
  eos::fst::DiskStat ds_c7;
  ASSERT_TRUE(ds_c7.Measure(tmp_path_c7));
  ASSERT_TRUE("24898" == ds_c7.values_t1["sda"]["readReq"]);
  // Check CentOS8 file format
  std::string tmp_path_c8 = std::tmpnam(nullptr);
  CreateCentOS8InputFile(tmp_path_c8);
  eos::fst::DiskStat ds_c8;
  ASSERT_TRUE(ds_c8.Measure(tmp_path_c8));
  ASSERT_TRUE("5132690" == ds_c8.values_t1["sdaw"]["readReq"]);
  ASSERT_EQ(0, unlink(tmp_path_c8.c_str()));
}
