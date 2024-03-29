// See LICENSE for license details.

#include "disasm.h"

/*const char* xpr_name[] = {
  "zero", "ra", "sp",  "gp",  "tp", "t0",  "t1",  "t2",
  "s0",   "s1", "a0",  "a1",  "a2", "a3",  "a4",  "a5",
  "a6",   "a7", "s2",  "s3",  "s4", "s5",  "s6",  "s7",
  "s8",   "s9", "s10", "s11", "t3", "t4",  "t5",  "t6",
  "x32",  "x33",  "x34",  "x35",  "x36",  "x37",  "x38",  "x39",  "x40",  "x41",  "x42",  "x43",  "x44",  "x45",  "x46",  "x47",  "x48",  "x49",  "x50",  "x51",  "x52",  "x53",  "x54",  "x55",  "x56",  "x57",  "x58",  "x59",  "x60",  "x61",  "x62",  "x63",  "x64",  "x65",  "x66",  "x67",  "x68",  "x69",  "x70",  "x71",  "x72",  "x73",  "x74",  "x75",  "x76",  "x77",  "x78",  "x79",  "x80",  "x81",  "x82",  "x83",  "x84",  "x85",  "x86",  "x87",  "x88",  "x89",  "x90",  "x91",  "x92",  "x93",  "x94",  "x95",  "x96",  "x97",  "x98",  "x99",  "x100",  "x101",  "x102",  "x103",  "x104",  "x105",  "x106",  "x107",  "x108",  "x109",  "x110",  "x111",  "x112",  "x113",  "x114",  "x115",  "x116",  "x117",  "x118",  "x119",  "x120",  "x121",  "x122",  "x123",  "x124",  "x125",  "x126",  "x127",  "x128",  "x129",  "x130",  "x131",  "x132",  "x133",  "x134",  "x135",  "x136",  "x137",  "x138",  "x139",  "x140",  "x141",  "x142",  "x143",  "x144",  "x145",  "x146",  "x147",  "x148",  "x149",  "x150",  "x151",  "x152",  "x153",  "x154",  "x155",  "x156",  "x157",  "x158",  "x159",  "x160",  "x161",  "x162",  "x163",  "x164",  "x165",  "x166",  "x167",  "x168",  "x169",  "x170",  "x171",  "x172",  "x173",  "x174",  "x175",  "x176",  "x177",  "x178",  "x179",  "x180",  "x181",  "x182",  "x183",  "x184",  "x185",  "x186",  "x187",  "x188",  "x189",  "x190",  "x191",  "x192",  "x193",  "x194",  "x195",  "x196",  "x197",  "x198",  "x199",  "x200",  "x201",  "x202",  "x203",  "x204",  "x205",  "x206",  "x207",  "x208",  "x209",  "x210",  "x211",  "x212",  "x213",  "x214",  "x215",  "x216",  "x217",  "x218",  "x219",  "x220",  "x221",  "x222",  "x223",  "x224",  "x225",  "x226",  "x227",  "x228",  "x229",  "x230",  "x231",  "x232",  "x233",  "x234",  "x235",  "x236",  "x237",  "x238",  "x239",  "x240",  "x241",  "x242",  "x243",  "x244",  "x245",  "x246",  "x247",  "x248",  "x249",  "x250",  "x251",  "x252",  "x253",  "x254",  "x255"
};*/

const char* xpr_name[] = {
  "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
  "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
  "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
  "x24", "x25", "x26", "x27", "x28", "x29", "x30", "x31",
  "x32",  "x33",  "x34",  "x35",  "x36",  "x37",  "x38",  "x39",  "x40",  "x41",  "x42",  "x43",  "x44",  "x45",  "x46",  "x47",  "x48",  "x49",  "x50",  "x51",  "x52",  "x53",  "x54",  "x55",  "x56",  "x57",  "x58",  "x59",  "x60",  "x61",  "x62",  "x63",  "x64",  "x65",  "x66",  "x67",  "x68",  "x69",  "x70",  "x71",  "x72",  "x73",  "x74",  "x75",  "x76",  "x77",  "x78",  "x79",  "x80",  "x81",  "x82",  "x83",  "x84",  "x85",  "x86",  "x87",  "x88",  "x89",  "x90",  "x91",  "x92",  "x93",  "x94",  "x95",  "x96",  "x97",  "x98",  "x99",  "x100",  "x101",  "x102",  "x103",  "x104",  "x105",  "x106",  "x107",  "x108",  "x109",  "x110",  "x111",  "x112",  "x113",  "x114",  "x115",  "x116",  "x117",  "x118",  "x119",  "x120",  "x121",  "x122",  "x123",  "x124",  "x125",  "x126",  "x127",  "x128",  "x129",  "x130",  "x131",  "x132",  "x133",  "x134",  "x135",  "x136",  "x137",  "x138",  "x139",  "x140",  "x141",  "x142",  "x143",  "x144",  "x145",  "x146",  "x147",  "x148",  "x149",  "x150",  "x151",  "x152",  "x153",  "x154",  "x155",  "x156",  "x157",  "x158",  "x159",  "x160",  "x161",  "x162",  "x163",  "x164",  "x165",  "x166",  "x167",  "x168",  "x169",  "x170",  "x171",  "x172",  "x173",  "x174",  "x175",  "x176",  "x177",  "x178",  "x179",  "x180",  "x181",  "x182",  "x183",  "x184",  "x185",  "x186",  "x187",  "x188",  "x189",  "x190",  "x191",  "x192",  "x193",  "x194",  "x195",  "x196",  "x197",  "x198",  "x199",  "x200",  "x201",  "x202",  "x203",  "x204",  "x205",  "x206",  "x207",  "x208",  "x209",  "x210",  "x211",  "x212",  "x213",  "x214",  "x215",  "x216",  "x217",  "x218",  "x219",  "x220",  "x221",  "x222",  "x223",  "x224",  "x225",  "x226",  "x227",  "x228",  "x229",  "x230",  "x231",  "x232",  "x233",  "x234",  "x235",  "x236",  "x237",  "x238",  "x239",  "x240",  "x241",  "x242",  "x243",  "x244",  "x245",  "x246",  "x247",  "x248",  "x249",  "x250",  "x251",  "x252",  "x253",  "x254",  "x255"
};

const char* fpr_name[] = {
  "ft0", "ft1", "ft2",  "ft3",  "ft4", "ft5", "ft6",  "ft7",
  "fs0", "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",
  "fa6", "fa7", "fs2",  "fs3",  "fs4", "fs5", "fs6",  "fs7",
  "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"
};

const char* vr_name[] = {
  "v0",  "v1",  "v2",  "v3",  "v4",  "v5",  "v6",  "v7",
  "v8",  "v9",  "v10", "v11", "v12", "v13", "v14", "v15",
  "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
  "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
  "v32",  "v33",  "v34",  "v35",  "v36",  "v37",  "v38",  "v39",  "v40",  "v41",  "v42",  "v43",  "v44",  "v45",  "v46",  "v47",  "v48",  "v49",  "v50",  "v51",  "v52",  "v53",  "v54",  "v55",  "v56",  "v57",  "v58",  "v59",  "v60",  "v61",  "v62",  "v63",  "v64",  "v65",  "v66",  "v67",  "v68",  "v69",  "v70",  "v71",  "v72",  "v73",  "v74",  "v75",  "v76",  "v77",  "v78",  "v79",  "v80",  "v81",  "v82",  "v83",  "v84",  "v85",  "v86",  "v87",  "v88",  "v89",  "v90",  "v91",  "v92",  "v93",  "v94",  "v95",  "v96",  "v97",  "v98",  "v99",  "v100",  "v101",  "v102",  "v103",  "v104",  "v105",  "v106",  "v107",  "v108",  "v109",  "v110",  "v111",  "v112",  "v113",  "v114",  "v115",  "v116",  "v117",  "v118",  "v119",  "v120",  "v121",  "v122",  "v123",  "v124",  "v125",  "v126",  "v127",  "v128",  "v129",  "v130",  "v131",  "v132",  "v133",  "v134",  "v135",  "v136",  "v137",  "v138",  "v139",  "v140",  "v141",  "v142",  "v143",  "v144",  "v145",  "v146",  "v147",  "v148",  "v149",  "v150",  "v151",  "v152",  "v153",  "v154",  "v155",  "v156",  "v157",  "v158",  "v159",  "v160",  "v161",  "v162",  "v163",  "v164",  "v165",  "v166",  "v167",  "v168",  "v169",  "v170",  "v171",  "v172",  "v173",  "v174",  "v175",  "v176",  "v177",  "v178",  "v179",  "v180",  "v181",  "v182",  "v183",  "v184",  "v185",  "v186",  "v187",  "v188",  "v189",  "v190",  "v191",  "v192",  "v193",  "v194",  "v195",  "v196",  "v197",  "v198",  "v199",  "v200",  "v201",  "v202",  "v203",  "v204",  "v205",  "v206",  "v207",  "v208",  "v209",  "v210",  "v211",  "v212",  "v213",  "v214",  "v215",  "v216",  "v217",  "v218",  "v219",  "v220",  "v221",  "v222",  "v223",  "v224",  "v225",  "v226",  "v227",  "v228",  "v229",  "v230",  "v231",  "v232",  "v233",  "v234",  "v235",  "v236",  "v237",  "v238",  "v239",  "v240",  "v241",  "v242",  "v243",  "v244",  "v245",  "v246",  "v247",  "v248",  "v249",  "v250",  "v251",  "v252",  "v253",  "v254",  "v255"
};

const char* csr_name(int which) {
  switch (which) {
    #define DECLARE_CSR(name, number)  case number: return #name;
    #include "encoding.h"
    #undef DECLARE_CSR
  }
  return "unknown-csr";
}
