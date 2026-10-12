##### references to 006d6558
  from 0042ae1a in CFileTree__ctor_or_dtor_CIPLTree_CTreeItem @ 0042a700
// ==== 0042a700 CFileTree__ctor_or_dtor_CIPLTree_CTreeItem size=4485

int **** __thiscall
CFileTree__ctor_or_dtor_CIPLTree_CTreeItem(int ****param_1,int **param_2,int ****param_3)

{
  int **ppiVar1;
  int *piVar2;
  int ***pppiVar3;
  int iVar4;
  undefined4 *puVar5;
  int ****ppppiVar6;
  undefined4 unaff_EBP;
  int *unaff_ESI;
  undefined4 unaff_EDI;
  int ***pppiVar7;
  int *unaff_retaddr;
  int **ppiStack_e0;
  int ***pppiStack_dc;
  int **ppiStack_d8;
  int **ppiStack_d4;
  int ****ppppiStack_d0;
  int ****ppppiStack_cc;
  int ***pppiStack_c8;
  int ***pppiStack_c4;
  int ****ppppiStack_c0;
  int ****ppppiStack_bc;
  int ****ppppiStack_b8;
  int ****ppppiStack_b4;
  int ****ppppiStack_b0;
  int ****ppppiStack_ac;
  int ****ppppiStack_a8;
  int ****ppppiStack_a4;
  int ****ppppiStack_a0;
  int ***pppiStack_9c;
  int ****ppppiStack_98;
  int ****ppppiStack_94;
  int ***pppiStack_90;
  int **ppiStack_8c;
  int ***pppiStack_88;
  int *piStack_84;
  int **ppiStack_80;
  int ***pppiStack_7c;
  int **ppiStack_78;
  int ****ppppiStack_74;
  int ****ppppiStack_70;
  int ****ppppiStack_6c;
  int ***pppiStack_68;
  int **ppiStack_64;
  int ***pppiStack_60;
  int ***pppiStack_5c;
  int ***pppiStack_58;
  int ***pppiStack_54;
  int ***pppiStack_50;
  int ****ppppiStack_4c;
  int ***pppiStack_48;
  int **ppiStack_44;
  int ***local_40;
  int ***pppiVar8;
  int ***local_28;
  undefined1 uStack_24;
  int ***local_20;
  int *piStack_1c;
  int ***pppiStack_18;
  undefined1 uStack_14;
  int *piStack_10;
  int ***pppiStack_c;
  int ***pppiStack_8;
  undefined4 local_4;
  
  local_4 = 0xffffffff;
  pppiStack_8 = (int ***)&LAB_006a830e;
  pppiStack_c = ExceptionList;
  local_28 = (int ***)&local_40;
  pppiVar7 = (int ***)0x0;
  ppiStack_44 = (int **)0x0;
  local_40 = (int ***)0x0;
  pppiStack_48 = (int ***)0x42a733;
  ExceptionList = &pppiStack_c;
  local_20 = (int ***)param_1;
  FUN_0047e3e0();
  *param_1 = (int ***)&PTR_CIPLTree__vfn_0_006d6674;
  param_1[2] = (int ***)&PTR_LAB_006d6658;
  param_1[5] = (int ***)&DAT_007af578;
  LOCK();
  DAT_007af578 = DAT_007af578 + 1;
  UNLOCK();
  local_40 = (int ***)0x10;
  local_4._0_1_ = 1;
  local_4._1_3_ = 0;
  param_1[6] = (int ***)param_3;
  ppiStack_44 = (int **)0x42a770;
  param_3 = (int ****)FUN_006a3bdf();
  local_4._0_1_ = 2;
  if (param_3 != (int ****)0x0) {
    ppiStack_44 = (int **)0x42a788;
    local_40 = (int ***)param_1;
    pppiVar7 = (int ***)FUN_0048cdc0();
  }
  local_4._0_1_ = 1;
  ppiStack_44 = (int **)0x42a79a;
  local_40 = pppiVar7;
  FUN_004753d0();
  local_40 = (int ***)0x1;
  ppiStack_44 = (int **)0x42a7a2;
  FUN_00494940();
  local_40 = (int ***)0x1;
  ppiStack_44 = (int **)0x42a7aa;
  FUN_00494960();
  local_40 = (int ***)&DAT_006d38a0;
  ppiStack_44 = (int **)0x42a7b6;
  FUN_004015a0();
  local_40 = (int ***)0x14;
  ppiStack_44 = (int **)0x42a7bd;
  param_3 = (int ****)FUN_006a3bdf();
  local_4._0_1_ = 3;
  if (param_3 == (int ****)0x0) {
    pppiVar7 = (int ***)0x0;
  }
  else {
    local_28 = (int ***)&local_40;
    local_40 = (int ***)0x0;
    ppiStack_44 = (int **)0x0;
    pppiStack_48 = (int ***)0x42a7e3;
    pppiVar7 = (int ***)FUN_0047e3e0();
  }
  param_3 = &local_40;
  ppiStack_44 = (int **)0x0;
  local_4._0_1_ = 1;
  local_40 = (int ***)0x0;
  ppppiStack_4c = (int ****)0x42a804;
  pppiStack_48 = pppiVar7;
  FUN_0048cb10();
  local_40 = (int ***)0x10;
  ppiStack_44 = (int **)0x42a80b;
  param_3 = (int ****)FUN_006a3bdf();
  local_4._0_1_ = 4;
  if (param_3 != (int ****)0x0) {
    ppiStack_44 = (int **)0x42a823;
    local_40 = pppiVar7;
    FUN_0048cd40();
  }
  local_40 = (int ***)0x0;
  local_4._0_1_ = 1;
  ppiStack_44 = (int **)0x42a836;
  FUN_00494940();
  local_40 = (int ***)0x5;
  ppiStack_44 = (int **)0x42a83f;
  FUN_00494960();
  local_40 = (int ***)0x14;
  ppiStack_44 = (int **)0x42a846;
  param_3 = (int ****)FUN_006a3bdf();
  local_4._0_1_ = 5;
  if (param_3 == (int ****)0x0) {
    pppiVar7 = (int ***)0x0;
  }
  else {
    local_40 = (int ***)0x0;
    ppiStack_44 = (int **)0x42a85f;
    pppiVar7 = (int ***)FUN_0048d450();
  }
  local_40 = (int ***)0x6d6648;
  local_4._0_1_ = 1;
  param_1[7] = pppiVar7;
  ppiStack_44 = (int **)0x42a878;
  QString_fromAscii_maybe();
  local_40 = (int ***)&param_3;
  local_4._0_1_ = 6;
  ppiStack_44 = (int **)0x42a88a;
  FUN_0047e5a0();
  local_4._0_1_ = 1;
  LOCK();
  ppiVar1 = (int **)*param_3;
  *param_3 = (int ***)((int)ppiVar1 + -1);
  UNLOCK();
  if ((int **)((int)ppiVar1 + -1) == (int **)0x0) {
    local_40 = (int ***)param_3;
    ppiStack_44 = (int **)0x42a8a5;
    QString_freeData();
  }
  local_40 = (int ***)0x6d6030;
  ppiStack_44 = (int **)0x42a8b6;
  QString_fromAscii_maybe();
  local_40 = (int ***)&param_3;
  local_4._0_1_ = 7;
  ppiStack_44 = (int **)0x42a8c9;
  local_40 = (int ***)FUN_0048a2f0();
  local_4._0_1_ = 8;
  ppiStack_44 = (int **)0x42a8d7;
  FUN_0047e670();
  local_4._0_1_ = 7;
  local_40 = (int ***)0x42a8e5;
  FUN_00489890();
  local_4._0_1_ = 1;
  LOCK();
  ppiVar1 = (int **)*param_3;
  *param_3 = (int ***)((int)ppiVar1 + -1);
  UNLOCK();
  if ((int **)((int)ppiVar1 + -1) == (int **)0x0) {
    local_40 = (int ***)param_3;
    ppiStack_44 = (int **)0x42a900;
    QString_freeData();
  }
  local_40 = (int ***)0x6d6624;
  ppiStack_44 = (int **)0x42a911;
  QString_fromAscii_maybe();
  local_40 = (int ***)&param_3;
  local_4 = CONCAT31(local_4._1_3_,9);
  ppiStack_44 = (int **)0x42a923;
  FUN_00475620();
  pppiStack_8._0_1_ = 1;
  LOCK();
  piVar2 = *param_2;
  *param_2 = (int *)((int)piVar2 + -1);
  UNLOCK();
  if ((int *)((int)piVar2 + -1) == (int *)0x0) {
    ppiStack_44 = param_2;
    pppiStack_48 = (int ***)0x42a93e;
    QString_freeData();
  }
  ppiStack_44 = (int **)0x6d6624;
  pppiStack_48 = (int ***)0x42a94f;
  QString_fromAscii_maybe();
  ppiStack_44 = (int **)&param_2;
  pppiStack_8 = (int ***)CONCAT31(pppiStack_8._1_3_,10);
  pppiStack_48 = (int ***)0x42a961;
  FUN_00475660();
  pppiStack_c = (int ***)CONCAT31(pppiStack_c._1_3_,1);
  LOCK();
  iVar4 = *unaff_retaddr;
  *unaff_retaddr = iVar4 + -1;
  UNLOCK();
  if (iVar4 + -1 == 0) {
    ppppiStack_4c = (int ****)0x42a97c;
    QString_freeData();
  }
  pppiStack_48 = (int ***)&stack0xffffffd0;
  ppppiStack_4c = (int ****)0x42a98c;
  iVar4 = (*(code *)(*param_1[7])[0xc])();
  pppiVar7 = *(int ****)(iVar4 + 4);
  ppppiStack_4c = &local_28;
  pppiStack_50 = (int ***)0x42a99c;
  puVar5 = (undefined4 *)(*(code *)(*param_1[7])[0xc])();
  pppiStack_54 = (int ***)*puVar5;
  pppiStack_58 = (int ***)0x42a9a8;
  pppiStack_50 = pppiVar7;
  FUN_0047c2a0();
  pppiStack_60 = param_1[7];
  pppiStack_50 = (int ***)0x0;
  pppiStack_54 = (int ***)0x6d6618;
  pppiStack_5c = (int ***)0x6d3ae8;
  ppiStack_64 = (int **)0x42a9be;
  pppiStack_58 = (int ***)param_1;
  FUN_006398c0();
  pppiStack_8 = (int ***)&pppiStack_50;
  pppiStack_50 = (int ***)0x0;
  pppiStack_58 = param_1[7];
  pppiStack_54 = (int ***)0x0;
  pppiStack_5c = (int ***)0x42a9da;
  FUN_0048cb10();
  pppiStack_50 = (int ***)0x14;
  pppiStack_54 = (int ***)0x42a9e1;
  pppiStack_8 = (int ***)FUN_006a3bdf();
  uStack_14 = 0xb;
  if (pppiStack_8 == (int ***)0x0) {
    pppiVar7 = (int ***)0x0;
  }
  else {
    pppiStack_50 = (int ***)0x0;
    pppiStack_54 = (int ***)0x42a9fa;
    pppiVar7 = (int ***)FUN_0048d450();
  }
  pppiStack_50 = (int ***)&DAT_006d6020;
  uStack_14 = 1;
  param_1[8] = pppiVar7;
  pppiStack_54 = (int ***)0x42aa13;
  QString_fromAscii_maybe();
  pppiStack_50 = (int ***)&pppiStack_8;
  uStack_14 = 0xc;
  pppiStack_54 = (int ***)0x42aa25;
  FUN_0047e5a0();
  uStack_14 = 1;
  LOCK();
  ppiVar1 = *pppiStack_8;
  *pppiStack_8 = (int **)((int)ppiVar1 + -1);
  UNLOCK();
  if ((int **)((int)ppiVar1 + -1) == (int **)0x0) {
    pppiStack_50 = pppiStack_8;
    pppiStack_54 = (int ***)0x42aa40;
    QString_freeData();
  }
  pppiStack_50 = (int ***)0x6d6008;
  pppiStack_54 = (int ***)0x42aa51;
  QString_fromAscii_maybe();
  pppiStack_50 = (int ***)&pppiStack_8;
  uStack_14 = 0xd;
  pppiStack_54 = (int ***)0x42aa64;
  pppiStack_50 = (int ***)FUN_0048a2f0();
  uStack_14 = 0xe;
  pppiStack_54 = (int ***)0x42aa72;
  FUN_0047e670();
  uStack_14 = 0xd;
  pppiStack_50 = (int ***)0x42aa80;
  FUN_00489890();
  uStack_14 = 1;
  LOCK();
  ppiVar1 = *pppiStack_8;
  *pppiStack_8 = (int **)((int)ppiVar1 + -1);
  UNLOCK();
  if ((int **)((int)ppiVar1 + -1) == (int **)0x0) {
    pppiStack_50 = pppiStack_8;
    pppiStack_54 = (int ***)0x42aa9b;
    QString_freeData();
  }
  pppiStack_50 = (int ***)0x6d65e8;
  pppiStack_54 = (int ***)0x42aaac;
  QString_fromAscii_maybe();
  pppiStack_50 = (int ***)&pppiStack_8;
  uStack_14 = 0xf;
  pppiStack_54 = (int ***)0x42aabe;
  FUN_00475620();
  pppiStack_18._0_1_ = 1;
  LOCK();
  ppiVar1 = *pppiStack_c;
  *pppiStack_c = (int **)((int)ppiVar1 + -1);
  UNLOCK();
  if ((int **)((int)ppiVar1 + -1) == (int **)0x0) {
    pppiStack_54 = pppiStack_c;
    pppiStack_58 = (int ***)0x42aad9;
    QString_freeData();
  }
  pppiStack_54 = (int ***)0x6d65e8;
  pppiStack_58 = (int ***)0x42aaea;
  QString_fromAscii_maybe();
  pppiStack_54 = (int ***)&pppiStack_c;
  pppiStack_18 = (int ***)CONCAT31(pppiStack_18._1_3_,0x10);
  pppiStack_58 = (int ***)0x42aafc;
  FUN_00475660();
  piStack_1c = (int *)CONCAT31(piStack_1c._1_3_,1);
  LOCK();
  iVar4 = *piStack_10;
  *piStack_10 = iVar4 + -1;
  UNLOCK();
  if (iVar4 + -1 == 0) {
    pppiStack_58 = (int ***)piStack_10;
    pppiStack_5c = (int ***)0x42ab17;
    QString_freeData();
  }
  pppiStack_58 = (int ***)0x0;
  pppiStack_5c = (int ***)0x42ab24;
  FUN_0047c0f0();
  pppiStack_58 = (int ***)&stack0xffffffcc;
  pppiStack_5c = (int ***)0x42ab31;
  iVar4 = (*(code *)(*param_1[8])[0xc])();
  pppiVar7 = *(int ****)(iVar4 + 4);
  pppiStack_5c = &ppiStack_44;
  pppiStack_60 = (int ***)0x42ab41;
  puVar5 = (undefined4 *)(*(code *)(*param_1[8])[0xc])();
  ppiStack_64 = (int **)*puVar5;
  pppiStack_68 = (int ***)0x42ab4d;
  pppiStack_60 = pppiVar7;
  FUN_0047c2a0();
  ppppiStack_70 = (int ****)param_1[8];
  pppiStack_60 = (int ***)0x0;
  ppiStack_64 = (int **)0x6d65dc;
  ppppiStack_6c = (int ****)0x6d3ae8;
  ppppiStack_74 = (int ****)0x42ab63;
  pppiStack_68 = (int ***)param_1;
  FUN_006398c0();
  ppppiStack_74 = (int ****)0x14;
  ppiStack_78 = (int **)0x42ab6a;
  pppiStack_18 = (int ***)FUN_006a3bdf();
  uStack_24 = 0x11;
  if (pppiStack_18 == (int ***)0x0) {
    pppiVar7 = (int ***)0x0;
  }
  else {
    pppiStack_60 = (int ***)0x0;
    ppiStack_64 = (int **)0x42ab83;
    pppiVar7 = (int ***)FUN_0048d450();
  }
  pppiStack_60 = (int ***)0x6d5ff4;
  uStack_24 = 1;
  param_1[9] = pppiVar7;
  ppiStack_64 = (int **)0x42ab9c;
  QString_fromAscii_maybe();
  pppiStack_60 = (int ***)&pppiStack_18;
  uStack_24 = 0x12;
  ppiStack_64 = (int **)0x42abae;
  FUN_0047e5a0();
  uStack_24 = 1;
  LOCK();
  ppiVar1 = *pppiStack_18;
  *pppiStack_18 = (int **)((int)ppiVar1 + -1);
  UNLOCK();
  if ((int **)((int)ppiVar1 + -1) == (int **)0x0) {
    pppiStack_60 = pppiStack_18;
    ppiStack_64 = (int **)0x42abc9;
    QString_freeData();
  }
  pppiStack_60 = (int ***)0x6d5fe0;
  ppiStack_64 = (int **)0x42abda;
  QString_fromAscii_maybe();
  pppiStack_60 = (int ***)&pppiStack_18;
  uStack_24 = 0x13;
  ppiStack_64 = (int **)0x42abed;
  pppiStack_60 = (int ***)FUN_0048a2f0();
  uStack_24 = 0x14;
  ppiStack_64 = (int **)0x42abfb;
  FUN_0047e670();
  uStack_24 = 0x13;
  pppiStack_60 = (int ***)0x42ac09;
  FUN_00489890();
  uStack_24 = 1;
  LOCK();
  ppiVar1 = *pppiStack_18;
  *pppiStack_18 = (int **)((int)ppiVar1 + -1);
  UNLOCK();
  if ((int **)((int)ppiVar1 + -1) == (int **)0x0) {
    pppiStack_60 = pppiStack_18;
    ppiStack_64 = (int **)0x42ac24;
    QString_freeData();
  }
  pppiStack_60 = (int ***)0x6d65a8;
  ppiStack_64 = (int **)0x42ac35;
  QString_fromAscii_maybe();
  pppiStack_60 = (int ***)&pppiStack_18;
  uStack_24 = 0x15;
  ppiStack_64 = (int **)0x42ac47;
  FUN_00475620();
  local_28._0_1_ = 1;
  LOCK();
  iVar4 = *piStack_1c;
  *piStack_1c = iVar4 + -1;
  UNLOCK();
  if (iVar4 + -1 == 0) {
    ppiStack_64 = (int **)piStack_1c;
    pppiStack_68 = (int ***)0x42ac62;
    QString_freeData();
  }
  ppiStack_64 = (int **)0x6d65a8;
  pppiStack_68 = (int ***)0x42ac73;
  QString_fromAscii_maybe();
  ppiStack_64 = &piStack_1c;
  local_28 = (int ***)CONCAT31(local_28._1_3_,0x16);
  pppiStack_68 = (int ***)0x42ac85;
  FUN_00475660();
  LOCK();
  pppiVar7 = (int ***)*local_20;
  *local_20 = (int **)((int)pppiVar7 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar7 + -1) == (int ***)0x0) {
    pppiStack_68 = local_20;
    ppppiStack_6c = (int ****)0x42aca0;
    QString_freeData();
  }
  ppiStack_78 = (int **)param_1[9];
  pppiStack_68 = (int ***)0x0;
  ppppiStack_6c = (int ****)0x6d659c;
  ppppiStack_74 = (int ****)0x6d3ae8;
  pppiStack_7c = (int ***)0x42acb9;
  ppppiStack_70 = param_1;
  FUN_006398c0();
  pppiStack_68 = &ppiStack_44;
  ppppiStack_6c = (int ****)0x42acc9;
  iVar4 = (*(code *)(*param_1[9])[0xc])();
  ppppiVar6 = *(int *****)(iVar4 + 4);
  ppppiStack_6c = &pppiStack_54;
  ppppiStack_70 = (int ****)0x42acd9;
  puVar5 = (undefined4 *)(*(code *)(*param_1[9])[0xc])();
  ppppiStack_74 = (int ****)*puVar5;
  ppiStack_78 = (int **)0x42ace5;
  ppppiStack_70 = ppppiVar6;
  FUN_0047c2a0();
  ppppiStack_70 = (int ****)0x0;
  ppppiStack_74 = (int ****)0x42acef;
  FUN_0047c0f0();
  local_28 = (int ***)&ppppiStack_70;
  ppppiStack_70 = (int ****)0x0;
  ppiStack_78 = (int **)param_1[9];
  ppppiStack_74 = (int ****)0x0;
  pppiStack_7c = (int ***)0x42ad09;
  FUN_0048cb10();
  local_28 = (int ***)&ppppiStack_70;
  ppppiStack_70 = (int ****)0x0;
  ppiStack_78 = (int **)param_1[8];
  ppppiStack_74 = (int ****)0x0;
  pppiStack_7c = (int ***)0x42ad23;
  FUN_0048cb10();
  ppppiStack_70 = (int ****)0x14;
  ppppiStack_74 = (int ****)0x42ad2a;
  local_28 = (int ***)FUN_006a3bdf();
  if ((int ****)local_28 == (int ****)0x0) {
    pppiVar7 = (int ***)0x0;
  }
  else {
    ppppiStack_70 = (int ****)0x0;
    ppppiStack_74 = (int ****)0x42ad43;
    pppiVar7 = (int ***)FUN_0048d450();
  }
  ppppiStack_70 = (int ****)&DAT_006d5598;
  ppppiStack_74 = (int ****)0x42ad5b;
  QString_fromAscii_maybe();
  ppppiStack_70 = &local_28;
  ppppiStack_74 = (int ****)0x42ad6c;
  FUN_0047e5a0();
  LOCK();
  pppiVar3 = (int ***)*local_28;
  *local_28 = (int **)((int)pppiVar3 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar3 + -1) == (int ***)0x0) {
    ppppiStack_70 = (int ****)local_28;
    ppppiStack_74 = (int ****)0x42ad87;
    QString_freeData();
  }
  ppppiStack_70 = (int ****)0x6d6588;
  ppppiStack_74 = (int ****)0x42ad98;
  QString_fromAscii_maybe();
  ppppiStack_70 = &local_28;
  ppppiStack_74 = (int ****)0x42adab;
  ppppiStack_70 = (int ****)FUN_0048a2f0();
  ppppiStack_74 = (int ****)0x42adb8;
  FUN_0047e670();
  ppppiStack_70 = (int ****)0x42adc6;
  FUN_00489890();
  pppiVar8 = (int ***)CONCAT31((int3)((uint)unaff_EBP >> 8),1);
  LOCK();
  pppiVar3 = (int ***)*local_28;
  *local_28 = (int **)((int)pppiVar3 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar3 + -1) == (int ***)0x0) {
    ppppiStack_70 = (int ****)local_28;
    ppppiStack_74 = (int ****)0x42ade1;
    QString_freeData();
  }
  ppppiStack_70 = (int ****)&ppppiStack_4c;
  ppppiStack_74 = (int ****)0x42adf0;
  (*(code *)(*pppiVar7)[0xc])();
  ppppiStack_74 = &pppiStack_5c;
  ppiStack_78 = (int **)0x42ae03;
  puVar5 = (undefined4 *)(*(code *)(*pppiVar7)[0xc])();
  pppiStack_7c = (int ***)*puVar5;
  ppiStack_80 = (int **)0x42ae12;
  FUN_0047c2a0();
  ppiStack_78 = (int **)0x1;
  pppiStack_7c = (int ***)0x42ae1a;
  FUN_0047c0f0();
  ppiStack_78 = (int **)0x6d6558;
  pppiStack_7c = (int ***)0x42ae28;
  QString_fromAscii_maybe();
  ppiStack_78 = (int **)&stack0xffffffd0;
  pppiStack_7c = (int ***)0x42ae39;
  FUN_00475620();
  local_40._0_1_ = 1;
  LOCK();
  ppiVar1 = *pppiVar8;
  *pppiVar8 = (int **)((int)ppiVar1 + -1);
  UNLOCK();
  if ((int **)((int)ppiVar1 + -1) == (int **)0x0) {
    ppiStack_80 = (int **)0x42ae54;
    pppiStack_7c = pppiVar8;
    QString_freeData();
  }
  pppiStack_7c = (int ***)0x6d6558;
  ppiStack_80 = (int **)0x42ae65;
  QString_fromAscii_maybe();
  pppiStack_7c = (int ***)&stack0xffffffcc;
  local_40 = (int ***)CONCAT31(local_40._1_3_,0x1c);
  ppiStack_80 = (int **)0x42ae76;
  FUN_00475660();
  ppiStack_44._0_1_ = 1;
  LOCK();
  iVar4 = *unaff_ESI;
  *unaff_ESI = iVar4 + -1;
  UNLOCK();
  if (iVar4 + -1 == 0) {
    piStack_84 = (int *)0x42ae91;
    QString_freeData();
  }
  piStack_84 = (int *)0x42aea0;
  ppiStack_80 = (int **)CONCAT31((int3)((uint)unaff_EDI >> 8),0x1b);
  FUN_0063a760();
  ppiStack_80 = (int **)0x0;
  piStack_84 = (int *)0x6d5474;
  ppiStack_8c = (int **)0x6d3ae8;
  ppppiStack_94 = (int ****)0x42aeb3;
  pppiStack_90 = pppiVar7;
  pppiStack_88 = (int ***)param_1;
  FUN_006398c0();
  piStack_84 = (int *)0x0;
  ppiStack_80 = (int **)0x0;
  ppiStack_8c = (int **)0x42aecc;
  pppiStack_88 = pppiVar7;
  FUN_0048cb10();
  ppiStack_80 = (int **)0x1;
  piStack_84 = (int *)0x42aed4;
  FUN_0048cb00();
  ppiStack_80 = (int **)0x14;
  piStack_84 = (int *)0x42aedb;
  iVar4 = FUN_006a3bdf();
  ppiStack_44._0_1_ = 0x1d;
  if (iVar4 == 0) {
    pppiVar7 = (int ***)0x0;
  }
  else {
    ppiStack_80 = (int **)0x0;
    piStack_84 = (int *)0x42aef4;
    pppiVar7 = (int ***)CTree4__ctor_or_dtor();
  }
  ppiStack_80 = (int **)0xc;
  ppiStack_44._0_1_ = 1;
  param_1[0xe] = pppiVar7;
  piStack_84 = (int *)0x42af06;
  pppiVar7 = (int ***)FUN_006a3bdf();
  ppiStack_44._0_1_ = 0x1e;
  if (pppiVar7 == (int ***)0x0) {
    pppiVar7 = (int ***)0x0;
  }
  else {
    ppiStack_80 = (int **)0x0;
    piStack_84 = (int *)0x42af21;
    FUN_0064f1b0();
    *pppiVar7 = (int **)&PTR_CFileTree__vfn_0_006d612c;
    pppiVar7[2] = (int **)&DAT_007af5a0;
    LOCK();
    DAT_007af5a0 = DAT_007af5a0 + 1;
    UNLOCK();
  }
  param_1[0x11] = pppiVar7;
  ppiStack_44 = (int **)CONCAT31(ppiStack_44._1_3_,1);
  piStack_84 = (int *)0x42af52;
  ppiStack_80 = (int **)pppiVar7;
  (*(code *)(*param_1[0xe])[0x34])();
  piStack_84 = (int *)0x1;
  pppiStack_88 = (int ***)0x0;
  ppiStack_8c = (int **)0x42af5d;
  FUN_004af480();
  pppiStack_9c = param_1[0xe];
  ppiStack_8c = (int **)0x0;
  pppiStack_90 = (int ***)0x6d6548;
  ppppiStack_98 = (int ****)0x6d6528;
  ppppiStack_a0 = (int ****)0x42af73;
  ppppiStack_94 = param_1;
  FUN_006398c0();
  ppppiStack_b0 = (int ****)param_1[0xe];
  ppppiStack_a0 = (int ****)0x0;
  ppppiStack_a4 = (int ****)0x6d6618;
  ppppiStack_ac = (int ****)0x6d6028;
  ppppiStack_b4 = (int ****)0x42af89;
  ppppiStack_a8 = param_1;
  FUN_006398c0();
  pppiStack_c4 = param_1[0xe];
  ppppiStack_b4 = (int ****)0x0;
  ppppiStack_b8 = (int ****)0x6d659c;
  ppppiStack_c0 = (int ****)0x6d5fd4;
  pppiStack_c8 = (int ***)0x42af9f;
  ppppiStack_bc = param_1;
  FUN_006398c0();
  ppiStack_d8 = (int **)param_1[0xe];
  pppiStack_c8 = (int ***)0x0;
  ppppiStack_cc = (int ****)0x6d65dc;
  ppiStack_d4 = (int **)0x6d5ffc;
  pppiStack_dc = (int ***)0x42afb5;
  ppppiStack_d0 = param_1;
  FUN_006398c0();
  pppiStack_9c = param_1[0xe];
  ppiStack_8c = (int **)0x0;
  pppiStack_90 = (int ***)0x6d6500;
  ppppiStack_98 = (int ****)0x6d64e0;
  ppppiStack_a0 = (int ****)0x42afce;
  ppppiStack_94 = param_1;
  FUN_006398c0();
  ppiStack_8c = (int **)0x1;
  pppiStack_90 = (int ***)0x0;
  ppppiStack_94 = (int ****)0x42afdc;
  FUN_004af480();
  ppppiStack_94 = (int ****)0x42afe3;
  FUN_00428320();
  ppppiStack_4c = (int ****)&ppppiStack_94;
  ppppiStack_94 = (int ****)0x0;
  pppiStack_9c = param_1[0xe];
  ppppiStack_98 = (int ****)0x0;
  ppppiStack_a0 = (int ****)0x42b001;
  FUN_0048cb10();
  ppppiStack_94 = (int ****)0x14;
  ppppiStack_98 = (int ****)0x42b008;
  ppppiStack_4c = (int ****)FUN_006a3bdf();
  pppiStack_58._0_1_ = 0x1f;
  if (ppppiStack_4c == (int ****)0x0) {
    ppppiVar6 = (int ****)0x0;
  }
  else {
    pppiStack_50 = (int ***)&ppppiStack_94;
    ppppiStack_94 = (int ****)0x0;
    ppppiStack_98 = (int ****)0x0;
    pppiStack_9c = (int ***)0x42b02e;
    ppppiVar6 = (int ****)FUN_0047e3e0();
  }
  ppppiStack_4c = (int ****)&ppppiStack_94;
  ppppiStack_98 = (int ****)0x0;
  pppiStack_58._0_1_ = 1;
  ppppiStack_94 = (int ****)0x0;
  ppppiStack_a0 = (int ****)0x42b04f;
  pppiStack_9c = (int ***)ppppiVar6;
  FUN_0048cb10();
  ppppiStack_94 = (int ****)0x10;
  ppppiStack_98 = (int ****)0x42b056;
  ppppiStack_4c = (int ****)FUN_006a3bdf();
  pppiStack_58._0_1_ = 0x20;
  if (ppppiStack_4c != (int ****)0x0) {
    ppppiStack_98 = (int ****)0x42b06e;
    ppppiStack_94 = ppppiVar6;
    FUN_0048cd40();
  }
  ppppiStack_94 = (int ****)0x0;
  pppiStack_58._0_1_ = 1;
  ppppiStack_98 = (int ****)0x42b081;
  FUN_00494940();
  ppppiStack_94 = (int ****)0x5;
  ppppiStack_98 = (int ****)0x42b08a;
  FUN_00494960();
  ppppiStack_94 = (int ****)0x14;
  ppppiStack_98 = (int ****)0x42b091;
  ppppiStack_4c = (int ****)FUN_006a3bdf();
  pppiStack_58._0_1_ = 0x21;
  if (ppppiStack_4c == (int ****)0x0) {
    pppiVar7 = (int ***)0x0;
  }
  else {
    ppppiStack_94 = (int ****)0x0;
    ppppiStack_98 = (int ****)0x42b0aa;
    pppiVar7 = (int ***)FUN_0048d450();
  }
  ppppiStack_94 = (int ****)&DAT_006d6044;
  pppiStack_58._0_1_ = 1;
  param_1[10] = pppiVar7;
  ppppiStack_98 = (int ****)0x42b0c3;
  QString_fromAscii_maybe();
  ppppiStack_94 = (int ****)&ppppiStack_4c;
  pppiStack_58._0_1_ = 0x22;
  ppppiStack_98 = (int ****)0x42b0d5;
  FUN_0047e5a0();
  pppiStack_58._0_1_ = 1;
  LOCK();
  pppiVar7 = *ppppiStack_4c;
  *ppppiStack_4c = (int ***)((int)pppiVar7 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar7 + -1) == (int ***)0x0) {
    ppppiStack_94 = ppppiStack_4c;
    ppppiStack_98 = (int ****)0x42b0f0;
    QString_freeData();
  }
  ppppiStack_94 = (int ****)0x6d6030;
  ppppiStack_98 = (int ****)0x42b101;
  QString_fromAscii_maybe();
  ppppiStack_94 = (int ****)&ppppiStack_4c;
  pppiStack_58._0_1_ = 0x23;
  ppppiStack_98 = (int ****)0x42b114;
  ppppiStack_94 = (int ****)FUN_0048a2f0();
  pppiStack_58._0_1_ = 0x24;
  ppppiStack_98 = (int ****)0x42b122;
  FUN_0047e670();
  pppiStack_58._0_1_ = 0x23;
  ppppiStack_94 = (int ****)0x42b130;
  FUN_00489890();
  pppiStack_58._0_1_ = 1;
  LOCK();
  pppiVar7 = *ppppiStack_4c;
  *ppppiStack_4c = (int ***)((int)pppiVar7 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar7 + -1) == (int ***)0x0) {
    ppppiStack_94 = ppppiStack_4c;
    ppppiStack_98 = (int ****)0x42b14b;
    QString_freeData();
  }
  ppppiStack_94 = (int ****)0x6d64b8;
  ppppiStack_98 = (int ****)0x42b15c;
  QString_fromAscii_maybe();
  ppppiStack_94 = (int ****)&ppppiStack_4c;
  pppiStack_58 = (int ***)CONCAT31(pppiStack_58._1_3_,0x25);
  ppppiStack_98 = (int ****)0x42b16e;
  FUN_00475620();
  pppiStack_5c = (int ***)CONCAT31(pppiStack_5c._1_3_,1);
  LOCK();
  pppiVar7 = (int ***)*pppiStack_50;
  *pppiStack_50 = (int **)((int)pppiVar7 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar7 + -1) == (int ***)0x0) {
    ppppiStack_98 = (int ****)pppiStack_50;
    pppiStack_9c = (int ***)0x42b189;
    QString_freeData();
  }
  ppppiStack_98 = (int ****)0x6d64b8;
  pppiStack_9c = (int ***)0x42b19a;
  QString_fromAscii_maybe();
  ppppiStack_98 = &pppiStack_50;
  pppiStack_5c = (int ***)CONCAT31(pppiStack_5c._1_3_,0x26);
  pppiStack_9c = (int ***)0x42b1ac;
  FUN_00475660();
  pppiStack_60 = (int ***)CONCAT31(pppiStack_60._1_3_,1);
  LOCK();
  pppiVar7 = (int ***)*pppiStack_54;
  *pppiStack_54 = (int **)((int)pppiVar7 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar7 + -1) == (int ***)0x0) {
    pppiStack_9c = pppiStack_54;
    ppppiStack_a0 = (int ****)0x42b1c7;
    QString_freeData();
  }
  pppiStack_9c = &ppiStack_78;
  ppppiStack_a0 = (int ****)0x42b1d7;
  iVar4 = (*(code *)(*param_1[10])[0xc])();
  ppppiVar6 = *(int *****)(iVar4 + 4);
  ppppiStack_a0 = &pppiStack_88;
  ppppiStack_a4 = (int ****)0x42b1e7;
  puVar5 = (undefined4 *)(*(code *)(*param_1[10])[0xc])();
  ppppiStack_a8 = (int ****)*puVar5;
  ppppiStack_ac = (int ****)0x42b1f3;
  ppppiStack_a4 = ppppiVar6;
  FUN_0047c2a0();
  ppppiStack_b4 = (int ****)param_1[10];
  ppppiStack_a4 = (int ****)0x0;
  ppppiStack_a8 = (int ****)0x6d64ac;
  ppppiStack_b0 = (int ****)0x6d3ae8;
  ppppiStack_b8 = (int ****)0x42b209;
  ppppiStack_ac = param_1;
  FUN_006398c0();
  pppiStack_5c = (int ***)&ppppiStack_a4;
  ppppiStack_a4 = (int ****)0x0;
  ppppiStack_ac = (int ****)param_1[10];
  ppppiStack_a8 = (int ****)0x0;
  ppppiStack_b0 = (int ****)0x42b225;
  FUN_0048cb10();
  ppppiStack_a4 = (int ****)0x14;
  ppppiStack_a8 = (int ****)0x42b22c;
  pppiStack_5c = (int ***)FUN_006a3bdf();
  pppiStack_68._0_1_ = 0x27;
  if ((int ****)pppiStack_5c == (int ****)0x0) {
    pppiVar7 = (int ***)0x0;
  }
  else {
    ppppiStack_a4 = (int ****)0x0;
    ppppiStack_a8 = (int ****)0x42b245;
    pppiVar7 = (int ***)FUN_0048d450();
  }
  ppppiStack_a4 = (int ****)&DAT_006d6094;
  pppiStack_68._0_1_ = 1;
  param_1[0xb] = pppiVar7;
  ppppiStack_a8 = (int ****)0x42b25e;
  QString_fromAscii_maybe();
  ppppiStack_a4 = &pppiStack_5c;
  pppiStack_68._0_1_ = 0x28;
  ppppiStack_a8 = (int ****)0x42b270;
  FUN_0047e5a0();
  pppiStack_68._0_1_ = 1;
  LOCK();
  pppiVar7 = (int ***)*pppiStack_5c;
  *pppiStack_5c = (int **)((int)pppiVar7 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar7 + -1) == (int ***)0x0) {
    ppppiStack_a4 = (int ****)pppiStack_5c;
    ppppiStack_a8 = (int ****)0x42b28b;
    QString_freeData();
  }
  ppppiStack_a4 = (int ****)0x6d6080;
  ppppiStack_a8 = (int ****)0x42b29c;
  QString_fromAscii_maybe();
  ppppiStack_a4 = &pppiStack_5c;
  pppiStack_68._0_1_ = 0x29;
  ppppiStack_a8 = (int ****)0x42b2af;
  ppppiStack_a4 = (int ****)FUN_0048a2f0();
  pppiStack_68._0_1_ = 0x2a;
  ppppiStack_a8 = (int ****)0x42b2bd;
  FUN_0047e670();
  pppiStack_68._0_1_ = 0x29;
  ppppiStack_a4 = (int ****)0x42b2cb;
  FUN_00489890();
  pppiStack_68._0_1_ = 1;
  LOCK();
  pppiVar7 = (int ***)*pppiStack_5c;
  *pppiStack_5c = (int **)((int)pppiVar7 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar7 + -1) == (int ***)0x0) {
    ppppiStack_a4 = (int ****)pppiStack_5c;
    ppppiStack_a8 = (int ****)0x42b2e6;
    QString_freeData();
  }
  ppppiStack_a4 = (int ****)0x6d6478;
  ppppiStack_a8 = (int ****)0x42b2f7;
  QString_fromAscii_maybe();
  ppppiStack_a4 = &pppiStack_5c;
  pppiStack_68 = (int ***)CONCAT31(pppiStack_68._1_3_,0x2b);
  ppppiStack_a8 = (int ****)0x42b309;
  FUN_00475620();
  ppppiStack_6c._0_1_ = 1;
  LOCK();
  pppiVar7 = (int ***)*pppiStack_60;
  *pppiStack_60 = (int **)((int)pppiVar7 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar7 + -1) == (int ***)0x0) {
    ppppiStack_a8 = (int ****)pppiStack_60;
    ppppiStack_ac = (int ****)0x42b324;
    QString_freeData();
  }
  ppppiStack_a8 = (int ****)0x6d6478;
  ppppiStack_ac = (int ****)0x42b335;
  QString_fromAscii_maybe();
  ppppiStack_a8 = &pppiStack_60;
  ppppiStack_6c = (int ****)CONCAT31(ppppiStack_6c._1_3_,0x2c);
  ppppiStack_ac = (int ****)0x42b347;
  FUN_00475660();
  ppppiStack_70 = (int ****)CONCAT31(ppppiStack_70._1_3_,1);
  LOCK();
  piVar2 = *ppiStack_64;
  *ppiStack_64 = (int *)((int)piVar2 + -1);
  UNLOCK();
  if ((int *)((int)piVar2 + -1) == (int *)0x0) {
    ppppiStack_ac = (int ****)ppiStack_64;
    ppppiStack_b0 = (int ****)0x42b362;
    QString_freeData();
  }
  ppppiStack_ac = &pppiStack_88;
  ppppiStack_b0 = (int ****)0x42b372;
  iVar4 = (*(code *)(*param_1[0xb])[0xc])();
  ppppiVar6 = *(int *****)(iVar4 + 4);
  ppppiStack_b0 = (int ****)&ppppiStack_98;
  ppppiStack_b4 = (int ****)0x42b382;
  puVar5 = (undefined4 *)(*(code *)(*param_1[0xb])[0xc])();
  ppppiStack_b8 = (int ****)*puVar5;
  ppppiStack_bc = (int ****)0x42b38e;
  ppppiStack_b4 = ppppiVar6;
  FUN_0047c2a0();
  pppiStack_c4 = param_1[0xb];
  ppppiStack_b4 = (int ****)0x0;
  ppppiStack_b8 = (int ****)0x6d6468;
  ppppiStack_c0 = (int ****)0x6d3ae8;
  pppiStack_c8 = (int ***)0x42b3a4;
  ppppiStack_bc = param_1;
  FUN_006398c0();
  ppppiStack_6c = (int ****)&ppppiStack_b4;
  ppppiStack_b4 = (int ****)0x0;
  ppppiStack_bc = (int ****)param_1[0xb];
  ppppiStack_b8 = (int ****)0x0;
  ppppiStack_c0 = (int ****)0x42b3c0;
  FUN_0048cb10();
  ppppiStack_b4 = (int ****)0x14;
  ppppiStack_b8 = (int ****)0x42b3c7;
  ppppiStack_6c = (int ****)FUN_006a3bdf();
  ppiStack_78._0_1_ = 0x2d;
  if (ppppiStack_6c == (int ****)0x0) {
    pppiVar7 = (int ***)0x0;
  }
  else {
    ppppiStack_b4 = (int ****)0x0;
    ppppiStack_b8 = (int ****)0x42b3e0;
    pppiVar7 = (int ***)FUN_0048d450();
  }
  ppppiStack_b4 = (int ****)0x6d6068;
  ppiStack_78._0_1_ = 1;
  param_1[0xc] = pppiVar7;
  ppppiStack_b8 = (int ****)0x42b3f9;
  QString_fromAscii_maybe();
  ppppiStack_b4 = (int ****)&ppppiStack_6c;
  ppiStack_78._0_1_ = 0x2e;
  ppppiStack_b8 = (int ****)0x42b40b;
  FUN_0047e5a0();
  ppiStack_78._0_1_ = 1;
  LOCK();
  pppiVar7 = *ppppiStack_6c;
  *ppppiStack_6c = (int ***)((int)pppiVar7 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar7 + -1) == (int ***)0x0) {
    ppppiStack_b4 = ppppiStack_6c;
    ppppiStack_b8 = (int ****)0x42b426;
    QString_freeData();
  }
  ppppiStack_b4 = (int ****)0x6d6054;
  ppppiStack_b8 = (int ****)0x42b437;
  QString_fromAscii_maybe();
  ppppiStack_b4 = (int ****)&ppppiStack_6c;
  ppiStack_78._0_1_ = 0x2f;
  ppppiStack_b8 = (int ****)0x42b44a;
  ppppiStack_b4 = (int ****)FUN_0048a2f0();
  ppiStack_78._0_1_ = 0x30;
  ppppiStack_b8 = (int ****)0x42b458;
  FUN_0047e670();
  ppiStack_78._0_1_ = 0x2f;
  ppppiStack_b4 = (int ****)0x42b466;
  FUN_00489890();
  ppiStack_78._0_1_ = 1;
  LOCK();
  pppiVar7 = *ppppiStack_6c;
  *ppppiStack_6c = (int ***)((int)pppiVar7 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar7 + -1) == (int ***)0x0) {
    ppppiStack_b4 = ppppiStack_6c;
    ppppiStack_b8 = (int ****)0x42b481;
    QString_freeData();
  }
  ppppiStack_b4 = (int ****)0x6d6434;
  ppppiStack_b8 = (int ****)0x42b492;
  QString_fromAscii_maybe();
  ppppiStack_b4 = (int ****)&ppppiStack_6c;
  ppiStack_78 = (int **)CONCAT31(ppiStack_78._1_3_,0x31);
  ppppiStack_b8 = (int ****)0x42b4a4;
  FUN_00475620();
  pppiStack_7c._0_1_ = 1;
  LOCK();
  pppiVar7 = *ppppiStack_70;
  *ppppiStack_70 = (int ***)((int)pppiVar7 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar7 + -1) == (int ***)0x0) {
    ppppiStack_b8 = ppppiStack_70;
    ppppiStack_bc = (int ****)0x42b4bf;
    QString_freeData();
  }
  ppppiStack_b8 = (int ****)0x6d6434;
  ppppiStack_bc = (int ****)0x42b4d0;
  QString_fromAscii_maybe();
  ppppiStack_b8 = (int ****)&ppppiStack_70;
  pppiStack_7c = (int ***)CONCAT31(pppiStack_7c._1_3_,0x32);
  ppppiStack_bc = (int ****)0x42b4e2;
  FUN_00475660();
  ppiStack_80 = (int **)CONCAT31(ppiStack_80._1_3_,1);
  LOCK();
  pppiVar7 = *ppppiStack_74;
  *ppppiStack_74 = (int ***)((int)pppiVar7 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar7 + -1) == (int ***)0x0) {
    ppppiStack_bc = ppppiStack_74;
    ppppiStack_c0 = (int ****)0x42b4fd;
    QString_freeData();
  }
  ppppiStack_bc = (int ****)&ppppiStack_98;
  ppppiStack_c0 = (int ****)0x42b50d;
  iVar4 = (*(code *)(*param_1[0xc])[0xc])();
  pppiVar7 = *(int ****)(iVar4 + 4);
  ppppiStack_c0 = (int ****)&ppppiStack_a8;
  pppiStack_c4 = (int ***)0x42b51d;
  puVar5 = (undefined4 *)(*(code *)(*param_1[0xc])[0xc])();
  pppiStack_c8 = (int ***)*puVar5;
  ppppiStack_cc = (int ****)0x42b529;
  pppiStack_c4 = pppiVar7;
  FUN_0047c2a0();
  ppiStack_d4 = (int **)param_1[0xc];
  pppiStack_c4 = (int ***)0x0;
  pppiStack_c8 = (int ***)0x6d6428;
  ppppiStack_d0 = (int ****)0x6d3ae8;
  ppiStack_d8 = (int **)0x42b53f;
  ppppiStack_cc = param_1;
  FUN_006398c0();
  pppiStack_7c = (int ***)&pppiStack_c4;
  pppiStack_c4 = (int ***)0x0;
  ppppiStack_cc = (int ****)param_1[0xc];
  pppiStack_c8 = (int ***)0x0;
  ppppiStack_d0 = (int ****)0x42b55b;
  FUN_0048cb10();
  pppiStack_c4 = (int ***)0x14;
  pppiStack_c8 = (int ***)0x42b562;
  pppiStack_7c = (int ***)FUN_006a3bdf();
  pppiStack_88._0_1_ = 0x33;
  if (pppiStack_7c == (int ***)0x0) {
    pppiVar7 = (int ***)0x0;
  }
  else {
    pppiStack_c4 = (int ***)0x0;
    pppiStack_c8 = (int ***)0x42b57b;
    pppiVar7 = (int ***)FUN_0048d450();
  }
  pppiStack_c4 = (int ***)0x6d5ff4;
  pppiStack_88._0_1_ = 1;
  param_1[0xd] = pppiVar7;
  pppiStack_c8 = (int ***)0x42b594;
  QString_fromAscii_maybe();
  pppiStack_c4 = (int ***)&pppiStack_7c;
  pppiStack_88._0_1_ = 0x34;
  pppiStack_c8 = (int ***)0x42b5a6;
  FUN_0047e5a0();
  pppiStack_88._0_1_ = 1;
  LOCK();
  ppiVar1 = *pppiStack_7c;
  *pppiStack_7c = (int **)((int)ppiVar1 + -1);
  UNLOCK();
  if ((int **)((int)ppiVar1 + -1) == (int **)0x0) {
    pppiStack_c4 = pppiStack_7c;
    pppiStack_c8 = (int ***)0x42b5c1;
    QString_freeData();
  }
  pppiStack_c4 = (int ***)0x6d5fe0;
  pppiStack_c8 = (int ***)0x42b5d2;
  QString_fromAscii_maybe();
  pppiStack_c4 = (int ***)&pppiStack_7c;
  pppiStack_88._0_1_ = 0x35;
  pppiStack_c8 = (int ***)0x42b5e5;
  pppiStack_c4 = (int ***)FUN_0048a2f0();
  pppiStack_88._0_1_ = 0x36;
  pppiStack_c8 = (int ***)0x42b5f3;
  FUN_0047e670();
  pppiStack_88._0_1_ = 0x35;
  pppiStack_c4 = (int ***)0x42b601;
  FUN_00489890();
  pppiStack_88._0_1_ = 1;
  LOCK();
  ppiVar1 = *pppiStack_7c;
  *pppiStack_7c = (int **)((int)ppiVar1 + -1);
  UNLOCK();
  if ((int **)((int)ppiVar1 + -1) == (int **)0x0) {
    pppiStack_c4 = pppiStack_7c;
    pppiStack_c8 = (int ***)0x42b61c;
    QString_freeData();
  }
  pppiStack_c4 = (int ***)0x6d63ec;
  pppiStack_c8 = (int ***)0x42b62d;
  QString_fromAscii_maybe();
  pppiStack_c4 = (int ***)&pppiStack_7c;
  pppiStack_88 = (int ***)CONCAT31(pppiStack_88._1_3_,0x37);
  pppiStack_c8 = (int ***)0x42b63f;
  FUN_00475620();
  ppiStack_8c._0_1_ = 1;
  LOCK();
  piVar2 = *ppiStack_80;
  *ppiStack_80 = (int *)((int)piVar2 + -1);
  UNLOCK();
  if ((int *)((int)piVar2 + -1) == (int *)0x0) {
    pppiStack_c8 = (int ***)ppiStack_80;
    ppppiStack_cc = (int ****)0x42b65a;
    QString_freeData();
  }
  pppiStack_c8 = (int ***)0x6d63ec;
  ppppiStack_cc = (int ****)0x42b66b;
  QString_fromAscii_maybe();
  pppiStack_c8 = &ppiStack_80;
  ppiStack_8c = (int **)CONCAT31(ppiStack_8c._1_3_,0x38);
  ppppiStack_cc = (int ****)0x42b67d;
  FUN_00475660();
  pppiStack_90 = (int ***)CONCAT31(pppiStack_90._1_3_,1);
  LOCK();
  iVar4 = *piStack_84;
  *piStack_84 = iVar4 + -1;
  UNLOCK();
  if (iVar4 + -1 == 0) {
    ppppiStack_cc = (int ****)piStack_84;
    ppppiStack_d0 = (int ****)0x42b698;
    QString_freeData();
  }
  ppppiStack_cc = (int ****)&ppppiStack_a8;
  ppppiStack_d0 = (int ****)0x42b6a8;
  iVar4 = (*(code *)(*param_1[0xd])[0xc])();
  ppiVar1 = *(int ***)(iVar4 + 4);
  ppppiStack_d0 = (int ****)&ppppiStack_a4;
  ppiStack_d4 = (int **)0x42b6b8;
  puVar5 = (undefined4 *)(*(code *)(*param_1[0xd])[0xc])();
  ppiStack_d8 = (int **)*puVar5;
  pppiStack_dc = (int ***)0x42b6c4;
  ppiStack_d4 = ppiVar1;
  FUN_0047c2a0();
  ppiStack_d4 = (int **)0x0;
  ppiStack_d8 = (int **)0x6d63e0;
  ppiStack_e0 = (int **)0x6d3ae8;
  pppiStack_dc = (int ***)param_1;
  FUN_006398c0(param_1[0xd]);
  ppiStack_8c = (int **)&ppiStack_d4;
  ppiStack_d4 = (int **)0x0;
  pppiStack_dc = param_1[0xd];
  ppiStack_d8 = (int **)0x0;
  ppiStack_e0 = (int **)0x42b6f6;
  FUN_0048cb10();
  ppiStack_d4 = (int **)0x1;
  ppiStack_d8 = (int **)0x42b6fe;
  FUN_0048cb00();
  ppiStack_d4 = (int **)0x0;
  ppiStack_d8 = (int **)0x42b708;
  FUN_0047c0f0();
  ppiStack_d4 = (int **)0x0;
  ppiStack_d8 = (int **)0x42b712;
  FUN_0047c0f0();
  ppiStack_d4 = (int **)0x0;
  ppiStack_d8 = (int **)0x42b71c;
  FUN_0047c0f0();
  ppiStack_d4 = (int **)0x0;
  ppiStack_d8 = (int **)0x42b726;
  FUN_0047c0f0();
  ppiStack_d4 = (int **)0x14;
  ppiStack_d8 = (int **)0x42b72d;
  ppiStack_8c = (int **)FUN_006a3bdf();
  ppppiStack_98._0_1_ = 0x39;
  if (ppiStack_8c == (int **)0x0) {
    pppiVar7 = (int ***)0x0;
  }
  else {
    ppiStack_d4 = (int **)0x0;
    ppiStack_d8 = (int **)0x42b746;
    pppiVar7 = (int ***)CTree3__ctor_or_dtor();
  }
  ppiStack_d4 = (int **)0xc;
  ppppiStack_98._0_1_ = 1;
  param_1[0xf] = pppiVar7;
  ppiStack_d8 = (int **)0x42b758;
  pppiVar7 = (int ***)FUN_006a3bdf();
  ppppiStack_98._0_1_ = 0x3a;
  ppiStack_8c = (int **)pppiVar7;
  if (pppiVar7 == (int ***)0x0) {
    pppiVar7 = (int ***)0x0;
  }
  else {
    ppiStack_d4 = (int **)param_1[0xf];
    ppiStack_d8 = (int **)0x42b775;
    FUN_0064f1b0();
    *pppiVar7 = (int **)&PTR_LAB_006d61cc;
    pppiVar7[2] = (int **)&DAT_007af5a0;
    LOCK();
    DAT_007af5a0 = DAT_007af5a0 + 1;
    UNLOCK();
  }
  ppiStack_d4 = (int **)0x0;
  ppiStack_d8 = (int **)0x6d6468;
  ppiStack_e0 = (int **)0x6d63bc;
  ppppiStack_98 = (int ****)CONCAT31(ppppiStack_98._1_3_,1);
  param_1[0x10] = pppiVar7;
  pppiStack_dc = (int ***)param_1;
  FUN_006398c0(param_1[0xf]);
  FUN_006398c0(param_1[0xf],"2clicked(const QModelIndex &)",param_1,"1instClicked()",0);
  FUN_006398c0(param_1[0xf],"2pressed(const QModelIndex &)",param_1,
               "1instRightClick(const QModelIndex &)",0);
  FUN_006398c0(param_1[0xf],"2pdelete()",param_1,"1delItem()",0);
  ppiStack_d4 = (int **)0x0;
  ppiStack_d8 = (int **)0x6d64ac;
  ppiStack_e0 = (int **)0x6d6028;
  pppiStack_dc = (int ***)param_1;
  FUN_006398c0(param_1[0xf]);
  FUN_006398c0(param_1[0xf],"2pedit()",param_1,"1instClicked()",0);
  FUN_006398c0(param_1[0xf],"2pcopy()",param_1,"1copyItem()",0);
  ppiStack_d4 = (int **)param_1[0x10];
  ppiStack_d8 = (int **)0x42b847;
  (*(code *)(*param_1[0xf])[0x34])();
  ppiStack_d8 = (int **)0x1;
  pppiStack_dc = (int ***)0x0;
  ppiStack_e0 = (int **)0x42b852;
  FUN_004af480();
  ppppiStack_98 = (int ****)&ppiStack_e0;
  ppiStack_e0 = (int **)0x0;
  FUN_0048cb10(param_1[0xf],0);
  ExceptionList = ppppiStack_ac;
  return param_1;
}


  from 0042ae57 in CFileTree__ctor_or_dtor_CIPLTree_CTreeItem @ 0042a700
##### references to 006d56c0
  from 00420782 in CIPLFinder__ctor_or_dtor @ 00420740
// ==== 00420740 CIPLFinder__ctor_or_dtor size=1447

int * __thiscall CIPLFinder__ctor_or_dtor(int *param_1,int *param_2,int **param_3)

{
  int iVar1;
  undefined4 uVar2;
  int *piVar3;
  int *unaff_retaddr;
  int *local_38;
  int *local_24;
  undefined1 *local_20;
  int local_1c;
  void *local_18;
  undefined4 local_14;
  uint local_10;
  void *pvStack_c;
  int *piStack_8;
  int *local_4;
  
  local_4 = (int *)0xffffffff;
  piStack_8 = (int *)&LAB_006a7205;
  pvStack_c = ExceptionList;
  local_20 = (undefined1 *)&local_38;
  local_38 = (int *)0x3103;
  ExceptionList = &pvStack_c;
  local_24 = param_1;
  FUN_004acd00(param_2);
  param_1[5] = (int)param_3;
  local_38 = (int *)0x6d56c0;
  local_4 = (int *)0x0;
  *param_1 = (int)&PTR_CIPLFinder__vfn_0_006d55ec;
  param_1[2] = (int)&PTR_LAB_006d55d0;
  QString_fromAscii_maybe();
  local_38 = (int *)&param_2;
  local_4._0_1_ = 1;
  FUN_00477910();
  local_4._0_1_ = 0;
  LOCK();
  iVar1 = *param_2;
  *param_2 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    local_38 = param_2;
    QString_freeData();
  }
  local_38 = (int *)0x65;
  FUN_0047c2a0(0x165);
  local_38 = (int *)0x65;
  FUN_0047c150(0x165);
  local_38 = (int *)0x0;
  FUN_004ac6b0();
  local_38 = (int *)0x14;
  param_2 = (int *)FUN_006a3bdf();
  local_4._0_1_ = 2;
  if (param_2 == (int *)0x0) {
    iVar1 = 0;
  }
  else {
    local_38 = param_1;
    iVar1 = FUN_0048d450();
  }
  local_38 = &local_1c;
  local_4._0_1_ = 0;
  param_1[6] = iVar1;
  local_1c = 0x110;
  local_18 = (void *)0xa;
  local_14 = 0x15f;
  local_10 = 0x20;
  FUN_0047a830();
  local_38 = (int *)0x6d5484;
  QString_fromAscii_maybe();
  local_38 = (int *)&param_2;
  local_4._0_1_ = 3;
  FUN_0047e5a0();
  local_4._0_1_ = 0;
  LOCK();
  iVar1 = *param_2;
  *param_2 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    local_38 = param_2;
    QString_freeData();
  }
  local_38 = (int *)0x0;
  FUN_0047c0f0();
  local_38 = (int *)0x0;
  FUN_006398c0(param_1[6],"2clicked()",param_1,"1findClicked()");
  param_1[9] = 0;
  param_1[10] = 0;
  param_2 = (int *)FUN_006a3bdf(0x14);
  local_4._0_1_ = 4;
  if (param_2 == (int *)0x0) {
    uVar2 = 0;
  }
  else {
    local_38 = param_1;
    uVar2 = FUN_0048d450();
  }
  local_38 = &local_1c;
  local_4._0_1_ = 0;
  local_1c = 0x110;
  local_18 = (void *)0x24;
  local_14 = 0x15f;
  local_10 = 0x3a;
  FUN_0047a830();
  local_38 = (int *)&DAT_006d4154;
  QString_fromAscii_maybe();
  local_38 = (int *)&param_2;
  local_4._0_1_ = 5;
  FUN_0047e5a0();
  local_4._0_1_ = 0;
  LOCK();
  iVar1 = *param_2;
  *param_2 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    local_38 = param_2;
    QString_freeData();
  }
  local_38 = (int *)0x0;
  FUN_006398c0(uVar2,"2clicked()",param_1,"1reject()");
  param_2 = (int *)FUN_006a3bdf(0x14);
  local_4._0_1_ = 6;
  if (param_2 != (int *)0x0) {
    param_3 = &local_38;
    local_38 = (int *)0x0;
    FUN_004bf9a0(param_1);
  }
  local_38 = (int *)0x6d5468;
  local_4._0_1_ = 0;
  QString_fromAscii_maybe();
  local_38 = (int *)&param_2;
  local_4._0_1_ = 7;
  FUN_004bfab0();
  local_4._0_1_ = 0;
  LOCK();
  iVar1 = *param_2;
  *param_2 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    local_38 = param_2;
    QString_freeData();
  }
  local_38 = &local_1c;
  local_1c = 5;
  local_18 = (void *)0x1;
  local_14 = 0x10d;
  local_10 = 0x17;
  FUN_0047a830();
  local_38 = (int *)0x14;
  param_2 = (int *)FUN_006a3bdf();
  local_4._0_1_ = 8;
  if (param_2 == (int *)0x0) {
    iVar1 = 0;
  }
  else {
    local_38 = param_1;
    iVar1 = FUN_004ccb10();
  }
  local_38 = (int *)0x7;
  local_4._0_1_ = 0;
  param_1[0xb] = iVar1;
  FUN_004779f0();
  local_38 = (int *)0x6d5444;
  QString_fromAscii_maybe();
  local_38 = (int *)&param_2;
  local_4 = (int *)CONCAT31(local_4._1_3_,9);
  FUN_00475620();
  piStack_8 = (int *)((uint)piStack_8 & 0xffffff00);
  LOCK();
  iVar1 = *unaff_retaddr;
  *unaff_retaddr = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(unaff_retaddr);
  }
  FUN_006398c0(param_1[0xb],"2textChanged(QString)",param_1,"1textChanged(QString)",0);
  local_20 = (undefined1 *)0x5;
  local_1c = 0x19;
  local_18 = (void *)0x10d;
  local_14 = 0x2f;
  FUN_0047a830(&local_20);
  piVar3 = (int *)FUN_006a3bdf(0x14);
  piStack_8._0_1_ = 10;
  if (piVar3 != (int *)0x0) {
    FUN_004d1110(param_1);
  }
  piStack_8._0_1_ = 0;
  QString_fromAscii_maybe("Filter By");
  piStack_8._0_1_ = 0xb;
  FUN_004d0fa0(&stack0x00000000);
  piStack_8._0_1_ = 0;
  LOCK();
  iVar1 = *piVar3;
  *piVar3 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(piVar3);
  }
  local_20 = (undefined1 *)0x5;
  local_1c = 0x32;
  local_18 = (void *)0x10d;
  local_14 = 0x59;
  FUN_0047a830(&local_20);
  piVar3 = (int *)FUN_006a3bdf(0x14);
  piStack_8._0_1_ = 0xc;
  if (piVar3 == (int *)0x0) {
    iVar1 = 0;
  }
  else {
    iVar1 = FUN_004cfe30(param_1);
  }
  piStack_8._0_1_ = 0;
  param_1[7] = iVar1;
  QString_fromAscii_maybe(&DAT_006d5400);
  piStack_8._0_1_ = 0xd;
  FUN_0047e5a0(&stack0x00000000);
  piStack_8._0_1_ = 0;
  LOCK();
  iVar1 = *piVar3;
  *piVar3 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(piVar3);
  }
  QString_fromAscii_maybe("Search by object\'s name");
  piStack_8 = (int *)CONCAT31(piStack_8._1_3_,0xe);
  FUN_00475620(&stack0x00000000);
  pvStack_c = (void *)((uint)pvStack_c & 0xffffff00);
  LOCK();
  iVar1 = *local_4;
  *local_4 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_4);
  }
  local_1c = 0x59;
  local_24 = (int *)0xa;
  local_20 = (undefined1 *)0x41;
  local_18 = (void *)0x57;
  FUN_0047a830(&local_24);
  local_4 = (int *)FUN_006a3bdf(0x14);
  pvStack_c._0_1_ = 0xf;
  if (local_4 == (int *)0x0) {
    iVar1 = 0;
  }
  else {
    iVar1 = FUN_004cfe30(param_1);
  }
  pvStack_c._0_1_ = 0;
  param_1[8] = iVar1;
  QString_fromAscii_maybe(&DAT_006d53c0);
  pvStack_c._0_1_ = 0x10;
  FUN_0047e5a0(&local_4);
  pvStack_c._0_1_ = 0;
  LOCK();
  iVar1 = *local_4;
  *local_4 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_4);
  }
  QString_fromAscii_maybe("Search by object\'s ID");
  pvStack_c = (void *)CONCAT31(pvStack_c._1_3_,0x11);
  FUN_00475620(&local_4);
  local_10 = local_10 & 0xffffff00;
  LOCK();
  iVar1 = *piStack_8;
  *piStack_8 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(piStack_8);
  }
  local_24 = (int *)0x41;
  local_20 = (undefined1 *)0x81;
  local_1c = 0x57;
  FUN_0047a830(&stack0xffffffd8);
  FUN_0047fbf0(1);
  ExceptionList = local_18;
  return param_1;
}


##### references to 006d53a8
  from 00420c63 in CIPLFinder__ctor_or_dtor @ 00420740
  from 0041fc9f in CIDEFinder__ctor_or_dtor @ 0041f6b0
// ==== 0041f6b0 CIDEFinder__ctor_or_dtor size=1651

void ** __thiscall CIDEFinder__ctor_or_dtor(void **param_1,void **param_2,void ***param_3)

{
  int iVar1;
  void *pvVar2;
  undefined4 uVar3;
  int *piVar4;
  int *unaff_retaddr;
  void **local_38;
  void **local_24;
  undefined1 *local_20;
  void *local_1c;
  undefined4 local_18;
  uint local_14;
  uint local_10;
  int *piStack_c;
  int *piStack_8;
  int *local_4;
  
  local_4 = (int *)0xffffffff;
  piStack_8 = (int *)&LAB_006a70c0;
  piStack_c = ExceptionList;
  local_20 = (undefined1 *)&local_38;
  local_38 = (void **)0x3103;
  ExceptionList = &piStack_c;
  local_24 = param_1;
  FUN_004acd00(param_2);
  param_1[5] = param_3;
  local_38 = (void **)0x6d5580;
  local_4 = (int *)0x0;
  *param_1 = &PTR_CIDEFinder__vfn_0_006d54ac;
  param_1[2] = &PTR_LAB_006d5490;
  QString_fromAscii_maybe();
  local_38 = &param_2;
  local_4._0_1_ = 1;
  FUN_00477910();
  local_4._0_1_ = 0;
  LOCK();
  pvVar2 = *param_2;
  *param_2 = (void *)((int)pvVar2 + -1);
  UNLOCK();
  if ((void *)((int)pvVar2 + -1) == (void *)0x0) {
    local_38 = param_2;
    QString_freeData();
  }
  local_38 = (void **)0x65;
  FUN_0047c2a0(0x165);
  local_38 = (void **)0x65;
  FUN_0047c150(0x165);
  local_38 = (void **)0x0;
  FUN_004ac6b0();
  local_38 = (void **)0x14;
  param_2 = (void **)FUN_006a3bdf();
  local_4._0_1_ = 2;
  if (param_2 == (void **)0x0) {
    pvVar2 = (void *)0x0;
  }
  else {
    local_38 = param_1;
    pvVar2 = (void *)FUN_0048d450();
  }
  local_38 = &local_1c;
  local_4._0_1_ = 0;
  param_1[6] = pvVar2;
  local_1c = (void *)0x110;
  local_18 = 10;
  local_14 = 0x15f;
  local_10 = 0x20;
  FUN_0047a830();
  local_38 = (void **)0x6d5484;
  QString_fromAscii_maybe();
  local_38 = &param_2;
  local_4._0_1_ = 3;
  FUN_0047e5a0();
  local_4._0_1_ = 0;
  LOCK();
  pvVar2 = *param_2;
  *param_2 = (void *)((int)pvVar2 + -1);
  UNLOCK();
  if ((void *)((int)pvVar2 + -1) == (void *)0x0) {
    local_38 = param_2;
    QString_freeData();
  }
  local_38 = (void **)0x0;
  FUN_0047c0f0();
  local_38 = (void **)0x0;
  FUN_006398c0(param_1[6],"2clicked()",param_1,"1findClicked()");
  param_1[10] = (void *)0x0;
  param_1[0xb] = (void *)0x0;
  param_2 = (void **)FUN_006a3bdf(0x14);
  local_4._0_1_ = 4;
  if (param_2 == (void **)0x0) {
    uVar3 = 0;
  }
  else {
    local_38 = param_1;
    uVar3 = FUN_0048d450();
  }
  local_38 = &local_1c;
  local_4._0_1_ = 0;
  local_1c = (void *)0x110;
  local_18 = 0x24;
  local_14 = 0x15f;
  local_10 = 0x3a;
  FUN_0047a830();
  local_38 = (void **)&DAT_006d4154;
  QString_fromAscii_maybe();
  local_38 = &param_2;
  local_4._0_1_ = 5;
  FUN_0047e5a0();
  local_4._0_1_ = 0;
  LOCK();
  pvVar2 = *param_2;
  *param_2 = (void *)((int)pvVar2 + -1);
  UNLOCK();
  if ((void *)((int)pvVar2 + -1) == (void *)0x0) {
    local_38 = param_2;
    QString_freeData();
  }
  local_38 = (void **)0x0;
  FUN_006398c0(uVar3,"2clicked()",param_1,"1reject()");
  param_2 = (void **)FUN_006a3bdf(0x14);
  local_4._0_1_ = 6;
  if (param_2 != (void **)0x0) {
    param_3 = &local_38;
    local_38 = (void **)0x0;
    FUN_004bf9a0(param_1);
  }
  local_38 = (void **)0x6d5468;
  local_4._0_1_ = 0;
  QString_fromAscii_maybe();
  local_38 = &param_2;
  local_4._0_1_ = 7;
  FUN_004bfab0();
  local_4._0_1_ = 0;
  LOCK();
  pvVar2 = *param_2;
  *param_2 = (void *)((int)pvVar2 + -1);
  UNLOCK();
  if ((void *)((int)pvVar2 + -1) == (void *)0x0) {
    local_38 = param_2;
    QString_freeData();
  }
  local_38 = &local_1c;
  local_1c = (void *)0x5;
  local_18 = 1;
  local_14 = 0x10d;
  local_10 = 0x17;
  FUN_0047a830();
  local_38 = (void **)0x14;
  param_2 = (void **)FUN_006a3bdf();
  local_4._0_1_ = 8;
  if (param_2 == (void *)0x0) {
    pvVar2 = (void *)0x0;
  }
  else {
    local_38 = param_1;
    pvVar2 = (void *)FUN_004ccb10();
  }
  local_38 = (void **)0x6d5444;
  local_4._0_1_ = 0;
  param_1[0xc] = pvVar2;
  QString_fromAscii_maybe();
  local_38 = &param_2;
  local_4 = (int *)CONCAT31(local_4._1_3_,9);
  FUN_00475620();
  piStack_8 = (int *)((uint)piStack_8 & 0xffffff00);
  LOCK();
  iVar1 = *unaff_retaddr;
  *unaff_retaddr = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(unaff_retaddr);
  }
  FUN_004779f0(7);
  FUN_006398c0(param_1[0xc],"2textChanged(QString)",param_1,"1textChanged(QString)",0);
  local_20 = (undefined1 *)0x5;
  local_1c = (void *)0x19;
  local_18 = 0x10d;
  local_14 = 0x2f;
  FUN_0047a830(&local_20);
  piVar4 = (int *)FUN_006a3bdf(0x14);
  piStack_8._0_1_ = 10;
  if (piVar4 != (int *)0x0) {
    FUN_004d1110(param_1);
  }
  piStack_8._0_1_ = 0;
  QString_fromAscii_maybe("Filter By");
  piStack_8._0_1_ = 0xb;
  FUN_004d0fa0(&stack0x00000000);
  piStack_8._0_1_ = 0;
  LOCK();
  iVar1 = *piVar4;
  *piVar4 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(piVar4);
  }
  local_18 = 0x10d;
  local_20 = (undefined1 *)0x5;
  local_1c = (void *)0x32;
  local_14 = 0x59;
  FUN_0047a830(&local_20);
  piVar4 = (int *)FUN_006a3bdf(0x14);
  piStack_8._0_1_ = 0xc;
  if (piVar4 == (int *)0x0) {
    pvVar2 = (void *)0x0;
  }
  else {
    pvVar2 = (void *)FUN_004cfe30(param_1);
  }
  piStack_8._0_1_ = 0;
  param_1[7] = pvVar2;
  QString_fromAscii_maybe(&DAT_006d5400);
  piStack_8._0_1_ = 0xd;
  FUN_0047e5a0(&stack0x00000000);
  piStack_8._0_1_ = 0;
  LOCK();
  iVar1 = *piVar4;
  *piVar4 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(piVar4);
  }
  QString_fromAscii_maybe("Search by object\'s name");
  piStack_8 = (int *)CONCAT31(piStack_8._1_3_,0xe);
  FUN_00475620(&stack0x00000000);
  piStack_c = (int *)((uint)piStack_c & 0xffffff00);
  LOCK();
  iVar1 = *local_4;
  *local_4 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_4);
  }
  local_1c = (void *)0x59;
  local_24 = (void **)0xa;
  local_20 = (undefined1 *)0x41;
  local_18 = 0x57;
  FUN_0047a830(&local_24);
  local_4 = (int *)FUN_006a3bdf(0x14);
  piStack_c._0_1_ = 0xf;
  if (local_4 == (int *)0x0) {
    pvVar2 = (void *)0x0;
  }
  else {
    pvVar2 = (void *)FUN_004cfe30(param_1);
  }
  piStack_c._0_1_ = 0;
  param_1[8] = pvVar2;
  QString_fromAscii_maybe("Texture");
  piStack_c._0_1_ = 0x10;
  FUN_0047e5a0(&local_4);
  piStack_c._0_1_ = 0;
  LOCK();
  iVar1 = *local_4;
  *local_4 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_4);
  }
  QString_fromAscii_maybe("Search by object\'s texture");
  piStack_c = (int *)CONCAT31(piStack_c._1_3_,0x11);
  FUN_00475620(&local_4);
  local_10 = local_10 & 0xffffff00;
  LOCK();
  iVar1 = *piStack_8;
  *piStack_8 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(piStack_8);
  }
  local_24 = (void **)0x41;
  local_20 = (undefined1 *)0x9f;
  local_1c = (void *)0x57;
  FUN_0047a830(&stack0xffffffd8);
  piStack_8 = (int *)FUN_006a3bdf(0x14);
  local_10._0_1_ = 0x12;
  if (piStack_8 == (int *)0x0) {
    pvVar2 = (void *)0x0;
  }
  else {
    pvVar2 = (void *)FUN_004cfe30(param_1);
  }
  local_10._0_1_ = 0;
  param_1[9] = pvVar2;
  QString_fromAscii_maybe(&DAT_006d53c0);
  local_10._0_1_ = 0x13;
  FUN_0047e5a0(&piStack_8);
  local_10._0_1_ = 0;
  LOCK();
  iVar1 = *piStack_8;
  *piStack_8 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(piStack_8);
  }
  QString_fromAscii_maybe("Search by object\'s ID");
  local_10 = CONCAT31(local_10._1_3_,0x14);
  FUN_00475620(&piStack_8);
  local_14 = local_14 & 0xffffff00;
  LOCK();
  iVar1 = *piStack_c;
  *piStack_c = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(piStack_c);
  }
  local_24 = (void **)0xd1;
  local_20 = (undefined1 *)0x57;
  FUN_0047a830(&stack0xffffffd4);
  FUN_0047fbf0(1);
  ExceptionList = local_1c;
  return param_1;
}


##### references to 006d8474
  from 0044b802 in FUN_0044b3a0 @ 0044b3a0
// ==== 0044b3a0 FUN_0044b3a0 size=1239

void __thiscall FUN_0044b3a0(int *param_1,int *param_2,int param_3)

{
  int iVar1;
  undefined4 uVar2;
  int iVar3;
  int *piVar4;
  int *piVar5;
  int *local_10;
  void *local_c;
  undefined1 *puStack_8;
  int local_4;
  
  local_4 = 0xffffffff;
  puStack_8 = &LAB_006ab518;
  local_c = ExceptionList;
  ExceptionList = &local_c;
  *(undefined1 *)((int)param_1 + 0x159) = 0;
  local_10 = param_1;
  std::operator<<<std::char_traits<char>_>
            ((basic_ostream<char,struct_std::char_traits<char>_> *)cout_exref,"setItem()\n");
  param_1[0x26] = (int)param_2;
  param_1[0x27] = param_3;
  QString_fromAscii_maybe("<font color=\'blue\'>");
  local_4 = 0;
  if ((param_1[0x26] == 0) || (param_1[0x27] == 0)) {
    QString_fromAscii_maybe("<font color=\'blue\'>Item Instance</font>");
    local_4._0_1_ = 4;
    FUN_004bfab0(&param_2);
    local_4 = (uint)local_4._1_3_ << 8;
    LOCK();
    iVar1 = *param_2;
    *param_2 = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(param_2);
    }
  }
  else {
    FUN_00636320(param_1[0x26] + 4);
    local_4._0_1_ = 1;
    uVar2 = FUN_00635bc0(&param_2);
    local_4._0_1_ = 2;
    FUN_00630470(uVar2);
    local_4._0_1_ = 1;
    LOCK();
    iVar1 = *param_2;
    *param_2 = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(param_2);
    }
    local_4 = (uint)local_4._1_3_ << 8;
    FUN_006361b0();
    QString_assignLiteral_maybe("</font>");
    iVar1 = param_1[0x27];
    *(undefined1 *)(param_1 + 0x56) = 0;
    param_1[7] = *(int *)(iVar1 + 4);
    param_1[0x20] = *(int *)(iVar1 + 0x68);
    FUN_00630420(iVar1 + 8);
    iVar1 = param_1[0x27];
    param_1[10] = *(int *)(iVar1 + 0x10);
    param_1[0xb] = *(int *)(iVar1 + 0x14);
    param_1[0xc] = *(int *)(iVar1 + 0x18);
    param_1[0xd] = *(int *)(iVar1 + 0x1c);
    param_1[0xe] = *(int *)(iVar1 + 0x20);
    param_1[0xf] = *(int *)(iVar1 + 0x24);
    piVar4 = (int *)(iVar1 + 0x40);
    piVar5 = param_1 + 0x16;
    for (iVar3 = 8; iVar3 != 0; iVar3 = iVar3 + -1) {
      *piVar5 = *piVar4;
      piVar4 = piVar4 + 1;
      piVar5 = piVar5 + 1;
    }
    *(undefined2 *)(param_1 + 0x1e) = *(undefined2 *)(iVar1 + 0x60);
    param_1[9] = *(int *)(iVar1 + 0xc);
    FUN_00467f10(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    FUN_0047c0f0(1);
    if (*(byte *)(param_1[0x24] + 0x10) < 3) {
      FUN_0047c0f0(0);
      FUN_0047c0f0(0);
    }
    if (*(char *)(param_1[0x24] + 0x10) == '\x01') {
      FUN_0047c0f0(0);
    }
    if (*(short *)(param_1[0x27] + 100) == 1) {
      FUN_004015a0("<font color=\'blue\'>");
      FUN_00630470(param_1[0x26] + 4);
      QString_assignLiteral_maybe(" Stream ");
      uVar2 = FUN_00634180(&param_2,(int)*(short *)(param_1[0x27] + 0x74),10);
      local_4._0_1_ = 3;
      FUN_00630470(uVar2);
      local_4 = (uint)local_4._1_3_ << 8;
      LOCK();
      iVar1 = *param_2;
      *param_2 = iVar1 + -1;
      UNLOCK();
      if (iVar1 + -1 == 0) {
        QString_freeData(param_2);
      }
    }
    FUN_004bfab0(&local_10);
    FUN_00446da0();
    FUN_004424d0();
    FUN_00446c40();
  }
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_10;
  *local_10 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_10);
  }
  ExceptionList = local_c;
  return;
}


  from 00441f31 in FUN_004419d0 @ 004419d0
// ==== 004419d0 FUN_004419d0 size=1472

void FUN_004419d0(void)

{
  int iVar1;
  int *local_14;
  int *local_10;
  void *local_c;
  undefined1 *puStack_8;
  undefined4 local_4;
  
  local_4 = 0xffffffff;
  puStack_8 = &LAB_006aacb8;
  local_c = ExceptionList;
  ExceptionList = &local_c;
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  QString_fromAscii_maybe("<img src=\':/images/cancel.png\' />");
  local_4 = 0;
  FUN_004bfab0(&local_14);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_14;
  *local_14 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_14);
  }
  QString_fromAscii_maybe("<img src=\':/images/cancel.png\' />");
  local_4 = 1;
  FUN_004bfab0(&local_14);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_14;
  *local_14 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_14);
  }
  FUN_00467f10(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  QString_fromAscii_maybe(&DAT_006d38a0);
  local_4 = 2;
  FUN_004ce1a0(&local_14);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_14;
  *local_14 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_14);
  }
  FUN_0047c0f0(0);
  QString_fromAscii_maybe(&DAT_006d38a0);
  local_4 = 3;
  FUN_004ce1a0(&local_14);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_14;
  *local_14 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_14);
  }
  FUN_0047c0f0(0);
  QString_fromAscii_maybe(&DAT_006d38a0);
  local_4 = 4;
  FUN_004ce1a0(&local_14);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_14;
  *local_14 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_14);
  }
  FUN_0047c0f0(0);
  QString_fromAscii_maybe(&DAT_006d38a0);
  local_4 = 5;
  FUN_004ce1a0(&local_14);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_14;
  *local_14 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_14);
  }
  FUN_0047c0f0(0);
  QString_fromAscii_maybe(&DAT_006d38a0);
  local_4 = 6;
  FUN_004ce1a0(&local_14);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_14;
  *local_14 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_14);
  }
  FUN_0047c0f0(0);
  QString_fromAscii_maybe(&DAT_006d38a0);
  local_4 = 7;
  FUN_004ce1a0(&local_14);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_14;
  *local_14 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_14);
  }
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  QString_fromAscii_maybe(&DAT_006d38a0);
  local_4 = 8;
  FUN_004ce1a0(&local_14);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_14;
  *local_14 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_14);
  }
  QString_fromAscii_maybe(&DAT_006d38a0);
  local_4 = 9;
  FUN_004ce1a0(&local_14);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_14;
  *local_14 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_14);
  }
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  FUN_0047c0f0(0);
  QString_fromAscii_maybe(&DAT_006d38a0);
  local_4 = 10;
  FUN_004ce1a0(&local_14);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_14;
  *local_14 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_14);
  }
  FUN_0047c0f0(0);
  QString_fromAscii_maybe(&DAT_006d38a0);
  local_4 = 0xb;
  FUN_004ce1a0(&local_14);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_14;
  *local_14 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_14);
  }
  QString_fromAscii_maybe("<font color=\'blue\'>Item Instance</font>");
  local_4 = 0xc;
  FUN_004bfab0(&local_10);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_10;
  *local_10 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_10);
  }
  FUN_0047c0f0(0);
  ExceptionList = local_c;
  return;
}


  from 0044756f in CIPLViewer__ctor_or_dtor @ 00447360
// ==== 00447360 CIPLViewer__ctor_or_dtor size=16296
// decompile failed
##### references to 006da944
  from 0045089d in MainWindow__ctor_or_dtor @ 00450570
// ==== 00450570 MainWindow__ctor_or_dtor size=16900

/* WARNING: Heritage AFTER dead removal. Example location: r0x007af5a0 : 0x00450a37 */
/* WARNING: Restarted to delay deadcode elimination for space: ram */

int **** __thiscall MainWindow__ctor_or_dtor(int ****param_1,int ***param_2,int ***param_3)

{
  int **ppiVar1;
  char cVar2;
  int iVar3;
  int ***pppiVar4;
  int iVar5;
  int *piVar6;
  undefined4 *puVar7;
  undefined4 uVar8;
  int ****ppppiVar9;
  int ****ppppiVar10;
  int ****ppppiVar11;
  undefined1 *puVar12;
  int *piVar13;
  int ****ppppiVar14;
  int ****unaff_EBP;
  int *unaff_ESI;
  undefined1 **ppuVar15;
  int ****ppppiStack_90;
  int ***local_84;
  int ***pppiStack_68;
  undefined1 *apuStack_64 [2];
  undefined4 uStack_5c;
  int iStack_58;
  undefined1 *local_54;
  int ***local_50;
  int ***pppiStack_44;
  int iStack_40;
  undefined4 auStack_3c [2];
  undefined4 uStack_34;
  undefined1 auStack_2c [12];
  void *pvStack_20;
  undefined1 uStack_18;
  undefined4 uStack_14;
  undefined4 uStack_10;
  int *piStack_c;
  int ***pppiStack_8;
  int ***local_4;
  
  local_4 = (int ***)0xffffffff;
  pppiStack_8 = (int ***)&LAB_006ac7c8;
  piStack_c = ExceptionList;
  local_54 = (undefined1 *)&local_84;
  piVar13 = (int *)0x0;
  local_84 = (int ***)0x0;
  ExceptionList = &piStack_c;
  local_50 = (int ***)param_1;
  FUN_004843e0();
  param_1[5] = param_3;
  local_84 = (int ***)0x6d5314;
  local_4 = (int ***)0x0;
  *param_1 = (int ***)&PTR_MainWindow__vfn_0_006da9ac;
  param_1[2] = (int ***)&PTR_LAB_006da990;
  QString_fromAscii_maybe();
  local_84 = (int ***)&param_2;
  local_4._0_1_ = 1;
  FUN_00477910();
  local_4._0_1_ = 0;
  LOCK();
  ppiVar1 = *param_2;
  *param_2 = (int **)((int)ppiVar1 + -1);
  UNLOCK();
  if ((int **)((int)ppiVar1 + -1) == (int **)0x0) {
    local_84 = param_2;
    QString_freeData();
  }
  local_84 = (int ***)0x1;
  FUN_00479750();
  local_84 = (int ***)0x45062e;
  iVar3 = FUN_00497670();
  iVar3 = *(int *)(*(int *)(iVar3 + 0x10) + 0x18) - *(int *)(*(int *)(iVar3 + 0x10) + 0x10);
  local_54 = (undefined1 *)(iVar3 + 1);
  local_84 = (int ***)0x14;
  param_2 = (int ***)FUN_006a3bdf();
  local_4._0_1_ = 2;
  if (param_2 == (int ***)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    local_84 = (int ***)param_1;
    pppiVar4 = (int ***)FUN_004fdbe0();
  }
  local_84 = (int ***)0x0;
  ppppiStack_90 = (int ****)0x6da95c;
  local_4 = (int ***)((uint)local_4._1_3_ << 8);
  param_1[0xb] = pppiVar4;
  FUN_006398c0(pppiVar4);
  local_84 = param_1[0xb];
  FUN_00484020();
  iVar5 = FUN_006a3bdf();
  pppiStack_8._0_1_ = 3;
  if (iVar5 == 0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    pppiVar4 = (int ***)FUN_004fab50();
  }
  pppiStack_8._0_1_ = 0;
  param_1[0x24] = pppiVar4;
  FUN_004ff190();
  piVar6 = (int *)FUN_006a3bdf();
  pppiStack_8._0_1_ = 4;
  if (piVar6 == (int *)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x4506f5;
    pppiVar4 = (int ***)CIDEModel__ctor_or_dtor_CIDETree();
  }
  pppiStack_8._0_1_ = 0;
  param_1[7] = pppiVar4;
  QString_fromAscii_maybe();
  pppiStack_8._0_1_ = 5;
  ppppiStack_90 = (int ****)0x450734;
  FUN_004fac60();
  pppiStack_8._0_1_ = 0;
  LOCK();
  iVar5 = *piVar6;
  *piVar6 = iVar5 + -1;
  UNLOCK();
  if (iVar5 + -1 == 0) {
    QString_freeData();
  }
  piVar6 = (int *)FUN_006a3bdf();
  pppiStack_8._0_1_ = 6;
  if (piVar6 == (int *)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x45077f;
    pppiVar4 = (int ***)CFileTree__ctor_or_dtor_CIPLTree_CTreeItem();
  }
  pppiStack_8._0_1_ = 0;
  param_1[6] = pppiVar4;
  QString_fromAscii_maybe();
  pppiStack_8._0_1_ = 7;
  ppppiStack_90 = (int ****)0x4507be;
  FUN_004fac60();
  pppiStack_8._0_1_ = 0;
  LOCK();
  iVar5 = *piVar6;
  *piVar6 = iVar5 + -1;
  UNLOCK();
  if (iVar5 + -1 == 0) {
    QString_freeData();
  }
  piVar6 = (int *)FUN_006a3bdf();
  pppiStack_8._0_1_ = 8;
  if (piVar6 == (int *)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x0;
    pppiVar4 = (int ***)CMapList__ctor_or_dtor();
  }
  pppiStack_8._0_1_ = 0;
  param_1[8] = pppiVar4;
  QString_fromAscii_maybe();
  pppiStack_8._0_1_ = 9;
  ppppiStack_90 = (int ****)0x450849;
  FUN_004fac60();
  pppiStack_8._0_1_ = 0;
  LOCK();
  iVar5 = *piVar6;
  *piVar6 = iVar5 + -1;
  UNLOCK();
  if (iVar5 + -1 == 0) {
    QString_freeData();
  }
  piVar6 = (int *)FUN_006a3bdf();
  pppiStack_8._0_1_ = 10;
  if (piVar6 == (int *)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)param_1[5];
    pppiVar4 = (int ***)CIPLViewer__ctor_or_dtor(0);
  }
  pppiStack_8._0_1_ = 0;
  param_1[0x25] = pppiVar4;
  QString_fromAscii_maybe();
  pppiStack_8._0_1_ = 0xb;
  ppppiStack_90 = (int ****)0x4508de;
  FUN_004fac60();
  pppiStack_8._0_1_ = 0;
  LOCK();
  iVar5 = *piVar6;
  *piVar6 = iVar5 + -1;
  UNLOCK();
  if (iVar5 + -1 == 0) {
    QString_freeData();
  }
  iVar5 = FUN_006a3bdf();
  pppiStack_8._0_1_ = 0xc;
  if (iVar5 == 0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    param_2 = (int ***)&stack0xffffff78;
    ppppiStack_90 = (int ****)0x450931;
    pppiVar4 = (int ***)FUN_004843e0();
  }
  pppiStack_8._0_1_ = 0;
  param_1[10] = pppiVar4;
  FUN_0047bc70();
  FUN_004ff190();
  iVar5 = FUN_006a3bdf();
  pppiStack_8._0_1_ = 0xd;
  if (iVar5 == 0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x0;
    pppiVar4 = (int ***)CMapView_ctor();
  }
  pppiStack_8 = (int ***)((uint)pppiStack_8._1_3_ << 8);
  param_1[9] = pppiVar4;
  FUN_0047bc70();
  FUN_00484020();
  ppppiStack_90 = (int ****)0x6da92c;
  FUN_006398c0(param_1[9],"2statusText(QString)",param_1);
  FUN_006398c0(param_1[9],"2showNormal()",param_1,"1showNormal()",0);
  LOCK();
  DAT_007af5a0 = DAT_007af5a0 + 1;
  UNLOCK();
  piStack_c = (int *)CONCAT31(piStack_c._1_3_,0xe);
  if (((DAT_007af5a0 != 1) && (piVar6 = (int *)FUN_00635250(), piVar6 != (int *)0x0)) &&
     (*piVar6 == 0)) {
    ppppiStack_90 = (int ****)0x450a21;
    free(piVar6);
  }
  puVar7 = (undefined4 *)FUN_00634c70();
  *puVar7 = 0xf0;
  if (((DAT_007af5a0 != 1) && (piVar6 = (int *)FUN_00635250(), piVar6 != (int *)0x0)) &&
     (*piVar6 == 0)) {
    ppppiStack_90 = (int ****)0x450a53;
    free(piVar6);
  }
  piVar6 = (int *)FUN_00634c70();
  *piVar6 = iVar3 + -0xdb;
  ppppiStack_90 = (int ****)0x450a6e;
  FUN_004feac0();
  ppppiStack_90 = (int ****)0x450a78;
  (*(code *)(*param_1[9])[0xb])();
  ppppiStack_90 = (int ****)0x6da8e4;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0xf;
  pppiVar4 = (int ***)FUN_00484590();
  param_1[0x10] = pppiVar4;
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x450ad0;
  uVar8 = FUN_004ff470();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(uVar8,"2triggered()",param_1,"1hideToolBar()");
  apuStack_64[0] = (undefined1 *)FUN_006a3bdf(8);
  uStack_10._0_1_ = 0x10;
  if (apuStack_64[0] == (undefined1 *)0x0) {
    ppppiVar9 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6d3a1c;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x6d3a08;
    uStack_10 = CONCAT31(uStack_10._1_3_,0x11);
    QString_fromAscii_maybe();
    ppppiStack_90 = &pppiStack_8;
    uStack_10 = 0x12;
    uVar8 = FUN_0048a2f0();
    piVar13 = (int *)0x7;
    uStack_10 = 0x13;
    unaff_ESI = (int *)0x7;
    ppppiStack_90 = param_1;
    ppppiVar9 = (int ****)FUN_00488c90(uVar8,&local_4);
  }
  uStack_10 = 0x15;
  if (((uint)piVar13 & 4) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfffffffb);
    ppppiStack_90 = (int ****)0x450b99;
    FUN_00489890();
    unaff_ESI = piVar13;
  }
  uStack_10 = 0x14;
  if (((uint)piVar13 & 2) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfffffffd);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  uStack_10 = 0xe;
  if (((uint)piVar13 & 1) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfffffffe);
    LOCK();
    pppiVar4 = (int ***)*local_4;
    *local_4 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)local_4;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6da8ac;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x17;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da8ac;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x18;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_004f37e0(0x4000052,0,0);
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x19;
  FUN_004882f0();
  uStack_10._0_1_ = 0xe;
  ppppiStack_90 = (int ****)0x450cd7;
  FUN_0066f510();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(ppppiVar9,"2triggered()",param_1[9],"1resetView()");
  ppppiStack_90 = ppppiVar9;
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x8;
  apuStack_64[0] = (undefined1 *)FUN_006a3bdf();
  uStack_10._0_1_ = 0x1a;
  if (apuStack_64[0] == (undefined1 *)0x0) {
    ppppiVar9 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da88c;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x6d7ef4;
    uStack_10 = CONCAT31(uStack_10._1_3_,0x1b);
    QString_fromAscii_maybe();
    ppppiStack_90 = &pppiStack_8;
    uStack_10 = 0x1c;
    uVar8 = FUN_0048a2f0();
    piVar13 = (int *)((uint)piVar13 | 0x38);
    uStack_10 = 0x1d;
    ppppiStack_90 = param_1;
    ppppiVar9 = (int ****)FUN_00488c90(uVar8,&local_4);
    unaff_ESI = piVar13;
  }
  uStack_10 = 0x1f;
  pppiStack_68 = (int ***)ppppiVar9;
  if (((uint)piVar13 & 0x20) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xffffffdf);
    ppppiStack_90 = (int ****)0x450daf;
    FUN_00489890();
    unaff_ESI = piVar13;
  }
  uStack_10 = 0x1e;
  if (((uint)piVar13 & 0x10) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xffffffef);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  uStack_10 = 0xe;
  if (((uint)piVar13 & 8) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfffffff7);
    LOCK();
    pppiVar4 = (int ***)*local_4;
    *local_4 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)local_4;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6da878;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x21;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da878;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x22;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(ppppiVar9,"2triggered()",param_1,"1showIMGTool()");
  ppppiStack_90 = ppppiVar9;
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x8;
  apuStack_64[0] = (undefined1 *)FUN_006a3bdf();
  uStack_10._0_1_ = 0x23;
  if (apuStack_64[0] == (undefined1 *)0x0) {
    ppppiVar9 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6d42e4;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x6da850;
    uStack_10 = CONCAT31(uStack_10._1_3_,0x24);
    QString_fromAscii_maybe();
    ppppiStack_90 = &pppiStack_8;
    uStack_10 = 0x25;
    uVar8 = FUN_0048a2f0();
    piVar13 = (int *)((uint)piVar13 | 0x1c0);
    uStack_10 = 0x26;
    ppppiStack_90 = param_1;
    ppppiVar9 = (int ****)FUN_00488c90(uVar8,&local_4);
    unaff_ESI = piVar13;
  }
  uStack_10 = 0x28;
  if (((uint)piVar13 & 0x100) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfffffeff);
    ppppiStack_90 = (int ****)0x450f8f;
    FUN_00489890();
    unaff_ESI = piVar13;
  }
  uStack_10 = 0x27;
  if ((char)piVar13 < '\0') {
    piVar13 = (int *)((uint)piVar13 & 0xffffff7f);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  uStack_10 = 0xe;
  if (((uint)piVar13 & 0x40) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xffffffbf);
    LOCK();
    pppiVar4 = (int ***)*local_4;
    *local_4 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)local_4;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6da83c;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x2a;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da83c;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x2b;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(ppppiVar9,"2triggered()",param_1,"1showBookmarks()");
  ppppiStack_90 = ppppiVar9;
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x8;
  iVar3 = FUN_006a3bdf();
  uStack_10._0_1_ = 0x2c;
  if (iVar3 == 0) {
    ppppiVar9 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da81c;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x6da804;
    uStack_10 = CONCAT31(uStack_10._1_3_,0x2d);
    QString_fromAscii_maybe();
    ppppiStack_90 = &pppiStack_8;
    uStack_10 = 0x2e;
    uVar8 = FUN_0048a2f0();
    piVar13 = (int *)((uint)piVar13 | 0xe00);
    uStack_10 = 0x2f;
    ppppiStack_90 = param_1;
    ppppiVar9 = (int ****)FUN_00488c90(uVar8,&local_4);
    unaff_ESI = piVar13;
  }
  uStack_10 = 0x31;
  if (((uint)piVar13 & 0x800) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfffff7ff);
    ppppiStack_90 = (int ****)0x451178;
    FUN_00489890();
    unaff_ESI = piVar13;
  }
  uStack_10 = 0x30;
  if (((uint)piVar13 & 0x400) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfffffbff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  uStack_10 = 0xe;
  if (((uint)piVar13 & 0x200) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfffffdff);
    LOCK();
    pppiVar4 = (int ***)*local_4;
    *local_4 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)local_4;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6da7e8;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x33;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da7e8;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x34;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_004f37e0(0x100003a,0,0);
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x35;
  FUN_004882f0();
  uStack_10._0_1_ = 0xe;
  ppppiStack_90 = (int ****)0x4512bc;
  FUN_0066f510();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(ppppiVar9,"2triggered()",param_1,"1fullScreen()");
  ppppiStack_90 = ppppiVar9;
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x8;
  iStack_58 = FUN_006a3bdf();
  uStack_10._0_1_ = 0x36;
  if (iStack_58 == 0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da7c8;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x6da7b4;
    uStack_10 = CONCAT31(uStack_10._1_3_,0x37);
    QString_fromAscii_maybe();
    ppppiStack_90 = &pppiStack_8;
    uStack_10 = 0x38;
    uVar8 = FUN_0048a2f0();
    piVar13 = (int *)((uint)piVar13 | 0x7000);
    uStack_10 = 0x39;
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488c90(uVar8,&local_4);
    unaff_ESI = piVar13;
  }
  param_1[0x1f] = pppiVar4;
  uStack_10 = 0x3b;
  if (((uint)piVar13 & 0x4000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xffffbfff);
    ppppiStack_90 = (int ****)0x45139e;
    FUN_00489890();
    unaff_ESI = piVar13;
  }
  uStack_10 = 0x3a;
  if (((uint)piVar13 & 0x2000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xffffdfff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  uStack_10 = 0xe;
  if (((uint)piVar13 & 0x1000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xffffefff);
    LOCK();
    pppiVar4 = (int ***)*local_4;
    *local_4 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)local_4;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6da79c;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x3d;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da784;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x3e;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)param_1[0x1f];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x1;
  FUN_004885c0();
  ppppiStack_90 = (int ****)0x0;
  FUN_004885f0();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0x1f],"2toggled(bool)",param_1[9],"1lockdrag(bool)");
  ppppiStack_90 = (int ****)0x4514ea;
  FUN_00484c60();
  ppppiStack_90 = (int ****)0x8;
  iStack_58 = FUN_006a3bdf();
  uStack_10._0_1_ = 0x3f;
  if (iStack_58 == 0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da768;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x6da754;
    uStack_10 = CONCAT31(uStack_10._1_3_,0x40);
    QString_fromAscii_maybe();
    ppppiStack_90 = &pppiStack_8;
    uStack_10 = 0x41;
    uVar8 = FUN_0048a2f0();
    piVar13 = (int *)((uint)piVar13 | 0x38000);
    uStack_10 = 0x42;
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488c90(uVar8,&local_4);
    unaff_ESI = piVar13;
  }
  param_1[0x12] = pppiVar4;
  uStack_10 = 0x44;
  if (((uint)piVar13 & 0x20000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfffdffff);
    ppppiStack_90 = (int ****)0x4515b1;
    FUN_00489890();
    unaff_ESI = piVar13;
  }
  uStack_10 = 0x43;
  if (((uint)piVar13 & 0x10000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfffeffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  uStack_10 = 0xe;
  if ((char)((uint)piVar13 >> 8) < '\0') {
    piVar13 = (int *)((uint)piVar13 & 0xffff7fff);
    LOCK();
    pppiVar4 = (int ***)*local_4;
    *local_4 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)local_4;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_004f37e0(0x4000052,0,0);
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x46;
  FUN_004882f0();
  uStack_10._0_1_ = 0xe;
  ppppiStack_90 = (int ****)0x45165e;
  FUN_0066f510();
  ppppiStack_90 = (int ****)0x6da768;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x47;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da768;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x48;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0x12],"2triggered()",param_1[0x25],"1resetAngle()");
  ppppiStack_90 = (int ****)param_1[0x12];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x8;
  iStack_58 = FUN_006a3bdf();
  uStack_10._0_1_ = 0x49;
  if (iStack_58 == 0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da730;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x6da71c;
    uStack_10 = CONCAT31(uStack_10._1_3_,0x4a);
    QString_fromAscii_maybe();
    ppppiStack_90 = &pppiStack_8;
    uStack_10 = 0x4b;
    uVar8 = FUN_0048a2f0();
    piVar13 = (int *)((uint)piVar13 | 0x1c0000);
    uStack_10 = 0x4c;
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488c90(uVar8,&local_4);
    unaff_ESI = piVar13;
  }
  param_1[0x13] = pppiVar4;
  uStack_10 = 0x4e;
  if (((uint)piVar13 & 0x100000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xffefffff);
    ppppiStack_90 = (int ****)0x4517eb;
    FUN_00489890();
    unaff_ESI = piVar13;
  }
  uStack_10 = 0x4d;
  if (((uint)piVar13 & 0x80000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfff7ffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  uStack_10 = 0xe;
  if (((uint)piVar13 & 0x40000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfffbffff);
    LOCK();
    pppiVar4 = (int ***)*local_4;
    *local_4 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)local_4;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_004f37e0(0x4000050,0,0);
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x50;
  FUN_004882f0();
  uStack_10._0_1_ = 0xe;
  ppppiStack_90 = (int ****)0x45189c;
  FUN_0066f510();
  ppppiStack_90 = (int ****)0x6da700;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x51;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da700;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x52;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0x13],"2triggered()",param_1[0x25],"1ideClicked()");
  ppppiStack_90 = (int ****)param_1[0x13];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x8;
  iStack_58 = FUN_006a3bdf();
  uStack_10._0_1_ = 0x53;
  if (iStack_58 == 0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da6dc;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x6da6c8;
    uStack_10 = CONCAT31(uStack_10._1_3_,0x54);
    QString_fromAscii_maybe();
    ppppiStack_90 = &pppiStack_8;
    uStack_10 = 0x55;
    uVar8 = FUN_0048a2f0();
    piVar13 = (int *)((uint)piVar13 | 0xe00000);
    uStack_10 = 0x56;
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488c90(uVar8,&local_4);
    unaff_ESI = piVar13;
  }
  param_1[0x14] = pppiVar4;
  uStack_10 = 0x58;
  if (((uint)piVar13 & 0x800000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xff7fffff);
    ppppiStack_90 = (int ****)0x451a29;
    FUN_00489890();
    unaff_ESI = piVar13;
  }
  uStack_10 = 0x57;
  if (((uint)piVar13 & 0x400000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xffbfffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  uStack_10 = 0xe;
  if (((uint)piVar13 & 0x200000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xffdfffff);
    LOCK();
    pppiVar4 = (int ***)*local_4;
    *local_4 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)local_4;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6da6b0;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x5a;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da6b0;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x5b;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0x14],"2triggered()",param_1[0x25],"1moveClicked()");
  ppppiStack_90 = (int ****)param_1[0x14];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x8;
  iStack_58 = FUN_006a3bdf();
  uStack_10._0_1_ = 0x5c;
  if (iStack_58 == 0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)&DAT_006da698;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x6da684;
    uStack_10 = CONCAT31(uStack_10._1_3_,0x5d);
    QString_fromAscii_maybe();
    ppppiStack_90 = &pppiStack_8;
    uStack_10 = 0x5e;
    uVar8 = FUN_0048a2f0();
    piVar13 = (int *)((uint)piVar13 | 0x7000000);
    uStack_10 = 0x5f;
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488c90(uVar8,&local_4);
    unaff_ESI = piVar13;
  }
  param_1[0x15] = pppiVar4;
  uStack_10 = 0x61;
  if (((uint)piVar13 & 0x4000000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfbffffff);
    ppppiStack_90 = (int ****)0x451c2a;
    FUN_00489890();
    unaff_ESI = piVar13;
  }
  uStack_10 = 0x60;
  if (((uint)piVar13 & 0x2000000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfdffffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  uStack_10 = 0xe;
  if (((uint)piVar13 & 0x1000000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xfeffffff);
    LOCK();
    pppiVar4 = (int ***)*local_4;
    *local_4 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)local_4;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_004f37e0(0x400005a,0,0);
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 99;
  FUN_004882f0();
  uStack_10._0_1_ = 0xe;
  ppppiStack_90 = (int ****)0x451cdb;
  FUN_0066f510();
  ppppiStack_90 = (int ****)0x6da674;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 100;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da674;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x65;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0x15],"2triggered()",param_1,"1undoClicked()");
  ppppiStack_90 = (int ****)param_1[0x15];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x8;
  iStack_58 = FUN_006a3bdf();
  uStack_10._0_1_ = 0x66;
  if (iStack_58 == 0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)&DAT_006da65c;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x6da648;
    uStack_10 = CONCAT31(uStack_10._1_3_,0x67);
    QString_fromAscii_maybe();
    ppppiStack_90 = &pppiStack_8;
    uStack_10 = 0x68;
    uVar8 = FUN_0048a2f0();
    piVar13 = (int *)((uint)piVar13 | 0x38000000);
    uStack_10 = 0x69;
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488c90(uVar8,&local_4);
    unaff_ESI = piVar13;
  }
  param_1[0x16] = pppiVar4;
  uStack_10 = 0x6b;
  if (((uint)piVar13 & 0x20000000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xdfffffff);
    ppppiStack_90 = (int ****)0x451e62;
    FUN_00489890();
    unaff_ESI = piVar13;
  }
  uStack_10 = 0x6a;
  if (((uint)piVar13 & 0x10000000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xefffffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  uStack_10 = 0xe;
  if (((uint)piVar13 & 0x8000000) != 0) {
    piVar13 = (int *)((uint)piVar13 & 0xf7ffffff);
    LOCK();
    pppiVar4 = (int ***)*local_4;
    *local_4 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)local_4;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_004f37e0(0x4000059,0,0);
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x6d;
  FUN_004882f0();
  uStack_10._0_1_ = 0xe;
  ppppiStack_90 = (int ****)0x451f13;
  FUN_0066f510();
  ppppiStack_90 = (int ****)0x6da638;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x6e;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da638;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x6f;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0x16],"2triggered()",param_1,"1redoClicked()");
  ppppiStack_90 = (int ****)param_1[0x16];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x451fdb;
  FUN_00484c60();
  ppppiStack_90 = (int ****)0x8;
  iStack_58 = FUN_006a3bdf();
  uStack_10._0_1_ = 0x70;
  if (iStack_58 == 0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da618;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x6da604;
    uStack_10 = CONCAT31(uStack_10._1_3_,0x71);
    QString_fromAscii_maybe();
    ppppiStack_90 = &pppiStack_8;
    piVar13 = (int *)((uint)piVar13 | 0xc0000000);
    uStack_10 = 0x72;
    uVar8 = FUN_0048a2f0();
    uStack_10 = 0x73;
    unaff_EBP = (int ****)0x1;
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488c90(uVar8,&local_4);
    unaff_ESI = piVar13;
  }
  param_1[0x1a] = pppiVar4;
  uStack_10 = 0x75;
  if (((uint)unaff_EBP & 1) != 0) {
    unaff_EBP = (int ****)((uint)unaff_EBP & 0xfffffffe);
    ppppiStack_90 = (int ****)0x4520a0;
    FUN_00489890();
  }
  uStack_10 = 0x74;
  if ((int)piVar13 < 0) {
    piVar13 = (int *)((uint)piVar13 & 0x7fffffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_ESI = piVar13;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  uStack_10 = 0xe;
  if (((uint)piVar13 & 0x40000000) != 0) {
    LOCK();
    pppiVar4 = (int ***)*local_4;
    *local_4 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)local_4;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_004f37e0(0x4000054,0,0);
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x77;
  FUN_004882f0();
  uStack_10._0_1_ = 0xe;
  ppppiStack_90 = (int ****)0x452143;
  FUN_0066f510();
  ppppiStack_90 = (int ****)0x6da5e4;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x78;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da5e4;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x79;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0x1a],"2triggered()",param_1,"1setTopDown()");
  ppppiStack_90 = (int ****)param_1[0x1a];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x14;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0x7a;
  if (local_4 == (int ***)0x0) {
    ppppiVar10 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)&DAT_006d6ee8;
    QString_fromAscii_maybe();
    unaff_EBP = (int ****)((uint)unaff_EBP | 2);
    uStack_10 = CONCAT31(uStack_10._1_3_,0x7b);
    ppppiStack_90 = param_1;
    ppppiVar10 = (int ****)FUN_004e94b0(&pppiStack_8);
  }
  uStack_10 = 0xe;
  ppppiVar14 = unaff_EBP;
  if (((uint)unaff_EBP & 2) != 0) {
    ppppiVar14 = (int ****)((uint)unaff_EBP & 0xfffffffd);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x8;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0x7d;
  if (local_4 == (int ***)0x0) {
    ppppiVar11 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da5c4;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 4);
    uStack_10 = CONCAT31(uStack_10._1_3_,0x7e);
    ppppiStack_90 = param_1;
    ppppiVar11 = (int ****)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 4) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xfffffffb);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6d7ef4;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x80;
  ppppiStack_90 = (int ****)FUN_0048a2f0();
  uStack_10._0_1_ = 0x81;
  FUN_00488480();
  uStack_10._0_1_ = 0x80;
  ppppiStack_90 = (int ****)0x45235d;
  FUN_00489890();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da5b0;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x82;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da5b0;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x83;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_004f37e0(0x600004f,0,0);
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x84;
  FUN_004882f0();
  uStack_10._0_1_ = 0xe;
  ppppiStack_90 = (int ****)0x45245b;
  FUN_0066f510();
  ppppiStack_90 = ppppiVar11;
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(ppppiVar11,"2triggered()",param_1,"1openGame()");
  local_4 = (int ***)FUN_006a3bdf(8);
  uStack_10._0_1_ = 0x85;
  if (local_4 == (int ***)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da594;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 8);
    uStack_10 = CONCAT31(uStack_10._1_3_,0x86);
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  param_1[0x19] = pppiVar4;
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 8) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xfffffff7);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6d6008;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x88;
  ppppiStack_90 = (int ****)FUN_0048a2f0();
  uStack_10._0_1_ = 0x89;
  FUN_00488480();
  uStack_10._0_1_ = 0x88;
  ppppiStack_90 = (int ****)0x452548;
  FUN_00489890();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da580;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x8a;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da56c;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x8b;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_004f37e0(0x4000053,0,0);
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x8c;
  FUN_004882f0();
  uStack_10._0_1_ = 0xe;
  ppppiStack_90 = (int ****)0x452649;
  FUN_0066f510();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0x19],"2triggered()",param_1,"1saveChanges()");
  ppppiStack_90 = (int ****)param_1[0x19];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x8;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0x8d;
  if (local_4 == (int ***)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da54c;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x10);
    uStack_10 = CONCAT31(uStack_10._1_3_,0x8e);
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  param_1[0x18] = pppiVar4;
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x10) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xffffffef);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6da530;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x90;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da530;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x91;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da51c;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x92;
  ppppiStack_90 = (int ****)FUN_0048a2f0();
  uStack_10._0_1_ = 0x93;
  FUN_00488480();
  uStack_10._0_1_ = 0x92;
  ppppiStack_90 = (int ****)0x4527db;
  FUN_00489890();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_004f37e0(0x7000033,0,0);
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0x94;
  FUN_004882f0();
  uStack_10._0_1_ = 0xe;
  ppppiStack_90 = (int ****)0x452840;
  FUN_0066f510();
  ppppiStack_90 = (int ****)param_1[0x18];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0x18],"2triggered()",param_1,"1closeGame()");
  ppppiStack_90 = (int ****)0x0;
  FUN_00488660();
  ppppiStack_90 = (int ****)0x0;
  FUN_00488660();
  ppppiStack_90 = (int ****)0x8;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0x95;
  if (local_4 == (int ***)0x0) {
    ppppiVar11 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)&DAT_006d7e0c;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x20);
    uStack_10 = CONCAT31(uStack_10._1_3_,0x96);
    ppppiStack_90 = param_1;
    ppppiVar11 = (int ****)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x20) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xffffffdf);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(ppppiVar11,"2triggered()",param_1,"1exitClicked()");
  ppppiStack_90 = ppppiVar11;
  FUN_00479ea0();
  ppppiStack_90 = ppppiVar10;
  FUN_00484190();
  FUN_004f5b10();
  ppppiStack_90 = (int ****)0x14;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0x98;
  if (local_4 == (int ***)0x0) {
    ppppiVar10 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da4f4;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x0;
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x40);
    uStack_10 = CONCAT31(uStack_10._1_3_,0x99);
    ppppiVar10 = (int ****)FUN_004e94b0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x40) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xffffffbf);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)param_1[0x15];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)param_1[0x16];
  FUN_00479ea0();
  ppppiStack_90 = ppppiVar10;
  FUN_00484190();
  FUN_004f5b10();
  ppppiStack_90 = (int ****)0x14;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0x9b;
  if (local_4 == (int ***)0x0) {
    ppppiVar10 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da4ec;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x0;
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x80);
    uStack_10 = CONCAT31(uStack_10._1_3_,0x9c);
    ppppiVar10 = (int ****)FUN_004e94b0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  uStack_10 = 0xe;
  if ((char)ppppiVar14 < '\0') {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xffffff7f);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = ppppiVar10;
  FUN_00484190();
  FUN_004f5b10();
  ppppiStack_90 = (int ****)0x8;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0x9e;
  if (local_4 == (int ***)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da4e4;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x100);
    uStack_10 = CONCAT31(uStack_10._1_3_,0x9f);
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  param_1[0xf] = pppiVar4;
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x100) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xfffffeff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x1;
  FUN_004885c0();
  ppppiStack_90 = (int ****)0x1;
  FUN_004885f0();
  ppppiStack_90 = (int ****)0x0;
  FUN_004f37e0(0x4000054,0,0);
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0xa1;
  FUN_004882f0();
  uStack_10._0_1_ = 0xe;
  ppppiStack_90 = (int ****)0x452b40;
  FUN_0066f510();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0xf],"2triggered()",param_1,"1viewoptions()");
  ppppiStack_90 = (int ****)param_1[0xf];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x8;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xa2;
  if (local_4 == (int ***)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da4cc;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x200);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xa3);
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  param_1[0xe] = pppiVar4;
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x200) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xfffffdff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x1;
  FUN_004885c0();
  ppppiStack_90 = (int ****)0x1;
  FUN_004885f0();
  ppppiStack_90 = (int ****)0x0;
  FUN_004f37e0(0x4000042,0,0);
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0xa5;
  FUN_004882f0();
  uStack_10._0_1_ = 0xe;
  ppppiStack_90 = (int ****)0x452c41;
  FUN_0066f510();
  ppppiStack_90 = (int ****)param_1[0xe];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0xe],"2triggered()",param_1,"1viewoptions()");
  local_4 = (int ***)FUN_006a3bdf(8);
  uStack_10._0_1_ = 0xa6;
  if (local_4 == (int ***)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da4c0;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x400);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xa7);
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  param_1[0xd] = pppiVar4;
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x400) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xfffffbff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x1;
  FUN_004885c0();
  ppppiStack_90 = (int ****)0x1;
  FUN_004885f0();
  ppppiStack_90 = (int ****)param_1[0xd];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0xd],"2triggered()",param_1,"1viewoptions()");
  ppppiStack_90 = (int ****)0x452d2a;
  FUN_004e7ca0();
  ppppiStack_90 = (int ****)0x8;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xa9;
  if (local_4 == (int ***)0x0) {
    ppppiVar11 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da4ac;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x800);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xaa);
    ppppiStack_90 = param_1;
    ppppiVar11 = (int ****)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x800) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xfffff7ff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_004885f0();
  ppppiStack_90 = (int ****)0x1;
  FUN_004885c0();
  ppppiStack_90 = ppppiVar11;
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(ppppiVar11,"2toggled(bool)",param_1[9],"1toggleWireFrame(bool)");
  local_4 = (int ***)FUN_006a3bdf(8);
  uStack_10._0_1_ = 0xac;
  if (local_4 == (int ***)0x0) {
    ppppiVar11 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da484;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x1000);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xad);
    ppppiStack_90 = param_1;
    ppppiVar11 = (int ****)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x1000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xffffefff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x1;
  FUN_004885c0();
  ppppiStack_90 = (int ****)0x0;
  FUN_004885f0();
  ppppiStack_90 = ppppiVar11;
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(ppppiVar11,"2toggled(bool)",param_1[9],"1toggle2dfx(bool)");
  local_4 = (int ***)FUN_006a3bdf(8);
  uStack_10._0_1_ = 0xaf;
  if (local_4 == (int ***)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da464;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x2000);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xb0);
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  param_1[0x17] = pppiVar4;
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x2000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xffffdfff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x1;
  FUN_004885c0();
  ppppiStack_90 = (int ****)0x1;
  FUN_004885f0();
  ppppiStack_90 = (int ****)param_1[0x17];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0x17],"2toggled(bool)",param_1,"1mapChange()");
  local_4 = (int ***)FUN_006a3bdf(8);
  uStack_10._0_1_ = 0xb2;
  if (local_4 == (int ***)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da444;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x4000);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xb3);
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  param_1[0x11] = pppiVar4;
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x4000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xffffbfff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x1;
  FUN_004885c0();
  ppppiStack_90 = (int ****)0x1;
  FUN_004885f0();
  ppppiStack_90 = (int ****)param_1[0x11];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x453007;
  FUN_004e7ca0();
  ppppiStack_90 = (int ****)0x8;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xb5;
  if (local_4 == (int ***)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da430;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x8000);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xb6);
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  param_1[0x1b] = pppiVar4;
  uStack_10 = 0xe;
  if ((char)((uint)ppppiVar14 >> 8) < '\0') {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xffff7fff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6da40c;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0xb8;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da3e8;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0xb9;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x1;
  FUN_004885c0();
  ppppiStack_90 = (int ****)0x8;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xba;
  if (local_4 == (int ***)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da3d8;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x10000);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xbb);
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  param_1[0x1c] = pppiVar4;
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x10000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xfffeffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6da3b8;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0xbd;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da398;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0xbe;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x1;
  FUN_004885c0();
  ppppiStack_90 = (int ****)0x8;
  pppiStack_8 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xbf;
  if ((int ****)pppiStack_8 != (int ****)0x0) {
    ppppiStack_90 = param_1;
    FUN_004fbe50();
  }
  ppppiStack_90 = (int ****)param_1[0x1b];
  uStack_10._0_1_ = 0xe;
  FUN_004fbf90();
  ppppiStack_90 = (int ****)param_1[0x1c];
  FUN_004fbf90();
  ppppiStack_90 = (int ****)0x8;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xc0;
  if (local_4 == (int ***)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da384;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x20000);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xc1);
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  param_1[0x1d] = pppiVar4;
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x20000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xfffdffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6da35c;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0xc3;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da35c;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0xc4;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x1;
  FUN_004885c0();
  ppppiStack_90 = (int ****)0x8;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xc5;
  if (local_4 == (int ***)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da34c;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x40000);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xc6);
    ppppiStack_90 = param_1;
    pppiVar4 = (int ***)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  param_1[0x1e] = pppiVar4;
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x40000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xfffbffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6da334;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 200;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da334;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0xc9;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x1;
  FUN_004885c0();
  ppppiStack_90 = (int ****)0x8;
  pppiStack_8 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xca;
  if ((int ****)pppiStack_8 != (int ****)0x0) {
    ppppiStack_90 = param_1;
    FUN_004fbe50();
  }
  ppppiStack_90 = (int ****)param_1[0x1d];
  uStack_10._0_1_ = 0xe;
  FUN_004fbf90();
  ppppiStack_90 = (int ****)param_1[0x1e];
  FUN_004fbf90();
  ppppiStack_90 = (int ****)param_1[0x1e];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)param_1[0x1d];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x1;
  FUN_004885f0();
  ppppiStack_90 = (int ****)0x45357e;
  FUN_004e7ca0();
  ppppiStack_90 = (int ****)param_1[0x1c];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)param_1[0x1b];
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x1;
  FUN_004885f0();
  ppppiStack_90 = (int ****)0x4535a5;
  FUN_004e7ca0();
  ppppiStack_90 = ppppiVar9;
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[0x11],"2toggled(bool)",param_1,"1mapChange()");
  ppppiStack_90 = (int ****)param_1[0x1a];
  FUN_00479ea0();
  ppppiStack_90 = ppppiVar10;
  FUN_00484190();
  FUN_004f5b10();
  ppppiStack_90 = (int ****)0x14;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xcb;
  if ((int ****)local_4 == (int ****)0x0) {
    ppppiVar9 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6d7f14;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x0;
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x80000);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xcc);
    ppppiVar9 = (int ****)FUN_004e94b0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x80000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xfff7ffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = ppppiVar9;
  FUN_00484190();
  FUN_004f5b10();
  ppppiStack_90 = (int ****)0x8;
  iStack_58 = FUN_006a3bdf();
  uStack_10._0_1_ = 0xce;
  if (iStack_58 == 0) {
    ppppiVar9 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da328;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x6da850;
    uStack_10 = CONCAT31(uStack_10._1_3_,0xcf);
    QString_fromAscii_maybe();
    ppppiStack_90 = &pppiStack_8;
    uStack_10 = 0xd0;
    uVar8 = FUN_0048a2f0();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x700000);
    uStack_10 = 0xd1;
    ppppiStack_90 = param_1;
    ppppiVar9 = (int ****)FUN_00488c90(uVar8,&local_4);
    unaff_EBP = ppppiVar14;
  }
  uStack_10 = 0xd3;
  if (((uint)ppppiVar14 & 0x400000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xffbfffff);
    ppppiStack_90 = (int ****)0x453745;
    FUN_00489890();
    unaff_EBP = ppppiVar14;
  }
  uStack_10 = 0xd2;
  if (((uint)ppppiVar14 & 0x200000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xffdfffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    unaff_EBP = ppppiVar14;
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x100000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xffefffff);
    LOCK();
    pppiVar4 = (int ***)*local_4;
    *local_4 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)local_4;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x6da83c;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0xd5;
  FUN_00488530();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x6da83c;
  QString_fromAscii_maybe();
  ppppiStack_90 = &pppiStack_8;
  uStack_10._0_1_ = 0xd6;
  FUN_00488560();
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(ppppiVar9,"2triggered()",param_1,"1showBookmarks()");
  ppppiStack_90 = ppppiVar9;
  FUN_00479ea0();
  ppppiStack_90 = (int ****)pppiStack_68;
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x14;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xd7;
  if (local_4 == (int ***)0x0) {
    ppppiVar9 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)&DAT_006da320;
    QString_fromAscii_maybe();
    ppppiStack_90 = (int ****)0x0;
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x800000);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xd8);
    ppppiVar9 = (int ****)FUN_004e94b0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x800000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xff7fffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x8;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xda;
  if (local_4 == (int ***)0x0) {
    ppppiVar10 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da318;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x1000000);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xdb);
    ppppiStack_90 = param_1;
    ppppiVar10 = (int ****)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x1000000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xfeffffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(ppppiVar10,"2triggered()",param_1,"1showAbout()");
  ppppiStack_90 = ppppiVar10;
  FUN_00479ea0();
  ppppiStack_90 = ppppiVar9;
  FUN_00484190();
  FUN_004f5b10();
  ppppiStack_90 = (int ****)0x8;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xdd;
  if (local_4 == (int ***)0x0) {
    ppppiVar9 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da2f8;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x2000000);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xde);
    ppppiStack_90 = param_1;
    ppppiVar9 = (int ****)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x2000000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xfdffffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(ppppiVar9,"2triggered()",param_1,"1statClick()");
  ppppiStack_90 = ppppiVar9;
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x8;
  local_4 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xe0;
  if ((int ****)local_4 == (int ****)0x0) {
    ppppiVar9 = (int ****)0x0;
  }
  else {
    ppppiStack_90 = (int ****)0x6da2d4;
    QString_fromAscii_maybe();
    ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x4000000);
    uStack_10 = CONCAT31(uStack_10._1_3_,0xe1);
    ppppiStack_90 = param_1;
    ppppiVar9 = (int ****)FUN_00488bd0(&pppiStack_8);
    unaff_EBP = ppppiVar14;
  }
  uStack_10 = 0xe;
  if (((uint)ppppiVar14 & 0x4000000) != 0) {
    ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xfbffffff);
    LOCK();
    pppiVar4 = (int ***)*pppiStack_8;
    *pppiStack_8 = (int **)((int)pppiVar4 + -1);
    UNLOCK();
    if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
      ppppiStack_90 = (int ****)pppiStack_8;
      QString_freeData();
    }
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(ppppiVar9,"2triggered()",param_1,"1helpClick()");
  ppppiStack_90 = ppppiVar9;
  FUN_00479ea0();
  ppppiStack_90 = (int ****)0x14;
  param_1[0x23] = (int ***)0x12c;
  param_1[0x22] = (int ***)0x9;
  pppiStack_8 = (int ***)FUN_006a3bdf();
  uStack_10._0_1_ = 0xe3;
  if ((int ****)pppiStack_8 == (int ****)0x0) {
    pppiVar4 = (int ***)0x0;
  }
  else {
    local_4 = (int ***)&ppppiStack_90;
    ppppiStack_90 = (int ****)0x0;
    pppiVar4 = (int ***)FUN_004bf9a0(0);
  }
  ppppiStack_90 = (int ****)0x32;
  uStack_10._0_1_ = 0xe;
  param_1[0x20] = pppiVar4;
  FUN_004e19c0();
  ppppiStack_90 = (int ****)0x11;
  FUN_0047c150(200);
  ppppiStack_90 = (int ****)0x11;
  FUN_0047c2a0(200);
  pppiVar4 = param_1[0x20];
  ppppiStack_90 = (int ****)0x0;
  FUN_00484240(pppiVar4);
  FUN_004fb800(pppiVar4);
  ppppiStack_90 = (int ****)0x6d9364;
  QString_fromAscii_maybe();
  ppppiStack_90 = (int ****)0x0;
  ppppiVar9 = &pppiStack_8;
  uStack_10._0_1_ = 0xe4;
  FUN_00484240(ppppiVar9);
  FUN_004fb550(ppppiVar9);
  uStack_10._0_1_ = 0xe;
  LOCK();
  pppiVar4 = (int ***)*pppiStack_8;
  *pppiStack_8 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)pppiStack_8;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x453c0a;
  FUN_004419d0();
  ppppiStack_90 = (int ****)0x0;
  FUN_0047c0f0();
  ppppiStack_90 = (int ****)0x0;
  FUN_0047c0f0();
  ppppiStack_90 = (int ****)0x0;
  FUN_00488660();
  ppppiStack_90 = (int ****)0x0;
  FUN_00488660();
  ppppiStack_90 = (int ****)0x0;
  FUN_00488660();
  ppppiStack_90 = (int ****)0x453c3e;
  FUN_00484240();
  ppppiStack_90 = (int ****)0x1f02;
  glGetString();
  ppppiStack_90 = (int ****)&DAT_006d38a0;
  QString_fromAscii_maybe();
  ppppiStack_90 = (int ****)0x1f01;
  uStack_10._0_1_ = 0xe5;
  ppppiStack_90 = (int ****)glGetString();
  QString_assignLiteral_maybe();
  ppppiStack_90 = (int ****)&DAT_006da2c0;
  QString_fromAscii_maybe();
  ppppiStack_90 = (int ****)0x1;
  uStack_10._0_1_ = 0xe6;
  iVar3 = FUN_0062fb50(&local_4,0);
  pppiStack_8 = (int ***)CONCAT31(pppiStack_8._1_3_,iVar3 != -1);
  uStack_10 = CONCAT31(uStack_10._1_3_,0xe5);
  LOCK();
  pppiVar4 = (int ***)*local_4;
  *local_4 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)local_4;
    QString_freeData();
  }
  if ((char)pppiStack_8 != '\0') {
    ppppiStack_90 = (int ****)0x14;
    iStack_58 = FUN_006a3bdf();
    uStack_10._0_1_ = 0xe7;
    if (iStack_58 != 0) {
      ppppiStack_90 = (int ****)0x6da178;
      QString_fromAscii_maybe();
      ppppiStack_90 = (int ****)&DAT_006d9324;
      uStack_10 = CONCAT31(uStack_10._1_3_,0xe8);
      QString_fromAscii_maybe();
      apuStack_64[0] = (undefined1 *)&ppppiStack_90;
      ppppiStack_90 = (int ****)0x103;
      ppppiVar14 = (int ****)((uint)ppppiVar14 | 0x18000000);
      uStack_10 = 0xe9;
      FUN_004aba70(&pppiStack_8,&local_4,3,1,0,0,param_1);
      unaff_EBP = ppppiVar14;
    }
    uStack_10 = 0xea;
    if (((uint)ppppiVar14 & 0x10000000) != 0) {
      ppppiVar14 = (int ****)((uint)ppppiVar14 & 0xefffffff);
      LOCK();
      pppiVar4 = (int ***)*pppiStack_8;
      *pppiStack_8 = (int **)((int)pppiVar4 + -1);
      UNLOCK();
      unaff_EBP = ppppiVar14;
      if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
        ppppiStack_90 = (int ****)pppiStack_8;
        QString_freeData();
      }
    }
    uStack_10 = 0xe5;
    if (((uint)ppppiVar14 & 0x8000000) != 0) {
      LOCK();
      pppiVar4 = (int ***)*local_4;
      *local_4 = (int **)((int)pppiVar4 + -1);
      UNLOCK();
      if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
        ppppiStack_90 = (int ****)local_4;
        QString_freeData();
      }
    }
    ppppiStack_90 = (int ****)0x8c;
    FUN_0047c150(0x1c2);
    ppppiStack_90 = (int ****)0x8c;
    FUN_0047c2a0(0x1c2);
    ppppiStack_90 = (int ****)0x453e04;
    FUN_004ac780();
  }
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[6],"2showItem(int)",param_1,"1showItem(int)");
  FUN_006398c0(param_1[6],"2addItem(int)",param_1,"1addItem(int)",0);
  FUN_006398c0(param_1[6],"2refreshList()",param_1,"1refreshList()",0);
  FUN_006398c0(param_1[8],"2changeMouse(int)",param_1,"1changeMouse(int)",0);
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[8],"2setEditable(int)",param_1,"1setEditable(int)");
  FUN_006398c0(param_1[8],"2speedChanged(int)",param_1[9],"1speedChanged(int)",0);
  FUN_006398c0(param_1[0x25],"2enableToolbar(bool)",param_1,"1enableIPLBar(bool)",0);
  FUN_006398c0(param_1[9],"2showIDE(int)",param_1[7],"1showIDE(int)",0);
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[6],"2setContext(int)",param_1[9],"1setContext(int)");
  FUN_006398c0(param_1[6],"2redraw()",param_1[9],"1cancelAll()",0);
  FUN_006398c0(param_1[9],"2refreshIPL()",param_1[6],"1iplSelected()",0);
  FUN_006398c0(param_1[9],"2showItemTab()",param_1,"1showItemTab()",0);
  ppppiStack_90 = (int ****)0x0;
  FUN_006398c0(param_1[8],"2drawMap()",param_1,"1mapChange()");
  param_1[8][10] = (int **)param_1[9];
  ppppiStack_90 = (int ****)0x0;
  FUN_00488660();
  ppppiStack_90 = (int ****)0x0;
  FUN_00488660();
  (*param_1[5])[2] = (int *)param_1[0x15];
  ppppiStack_90 = (int ****)&DAT_006d9324;
  (*param_1[5])[3] = (int *)param_1[0x16];
  QString_fromAscii_maybe();
  ppppiStack_90 = (int ****)0x6d9fd4;
  uStack_10._0_1_ = 0xec;
  QString_fromAscii_maybe();
  ppppiStack_90 = (int ****)0x0;
  uStack_10._0_1_ = 0xed;
  FUN_0064f620(&local_4,&stack0xffffff88);
  uStack_10 = CONCAT31(uStack_10._1_3_,0xf0);
  LOCK();
  pppiVar4 = (int ***)*local_4;
  *local_4 = (int **)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = (int ****)local_4;
    QString_freeData();
  }
  uStack_10 = CONCAT31(uStack_10._1_3_,0xef);
  LOCK();
  pppiVar4 = *unaff_EBP;
  *unaff_EBP = (int ***)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    ppppiStack_90 = unaff_EBP;
    QString_freeData();
  }
  ppppiStack_90 = (int ****)0x0;
  (*(code *)(*param_1)[0xb])();
  QString_fromAscii_maybe("mainwindow/size");
  local_54 = (undefined1 *)0x80000000;
  uStack_5c = 0;
  uStack_14._0_1_ = 0xf2;
  FUN_006514e0(auStack_3c,&piStack_c,&uStack_5c);
  uStack_14._0_1_ = 0xf3;
  FUN_0063c2a0(&pppiStack_44);
  uStack_14._0_1_ = 0xf2;
  FUN_0063aa20();
  uStack_14._0_1_ = 0xf1;
  FUN_0063aa20();
  uStack_14._0_1_ = 0xef;
  LOCK();
  iVar3 = *piStack_c;
  *piStack_c = iVar3 + -1;
  UNLOCK();
  if (iVar3 + -1 == 0) {
    QString_freeData(piStack_c);
  }
  if (((int)pppiStack_44 < 1) || (iStack_40 < 1)) {
    FUN_00478f60();
  }
  else {
    FUN_0047a7f0(&pppiStack_44);
    QString_fromAscii_maybe("mainwindow/position");
    uStack_34 = 0x80000000;
    auStack_3c[0] = 0;
    uStack_14._0_1_ = 0xf5;
    FUN_006514e0(auStack_2c,&piStack_c,auStack_3c);
    uStack_14._0_1_ = 0xf6;
    FUN_0063c420(&uStack_5c);
    uStack_14._0_1_ = 0xf5;
    FUN_0063aa20();
    uStack_14._0_1_ = 0xf4;
    FUN_0063aa20();
    uStack_14._0_1_ = 0xef;
    LOCK();
    iVar3 = *piStack_c;
    *piStack_c = iVar3 + -1;
    UNLOCK();
    if (iVar3 + -1 == 0) {
      QString_freeData(piStack_c);
    }
    FUN_0047a6d0(&uStack_5c);
  }
  QString_fromAscii_maybe("controls/swidth");
  uStack_34 = 0x80000000;
  auStack_3c[0] = 0;
  uStack_14._0_1_ = 0xf8;
  FUN_006514e0(auStack_2c,&piStack_c,auStack_3c);
  uStack_14._0_1_ = 0xf9;
  piVar13 = (int *)FUN_0063c2a0(&uStack_5c);
  pppiStack_44 = (int ***)*piVar13;
  iStack_40 = piVar13[1];
  uStack_14._0_1_ = 0xf8;
  FUN_0063aa20();
  uStack_14._0_1_ = 0xf7;
  FUN_0063aa20();
  uStack_14._0_1_ = 0xef;
  LOCK();
  iVar3 = *piStack_c;
  *piStack_c = iVar3 + -1;
  UNLOCK();
  if (iVar3 + -1 == 0) {
    QString_freeData(piStack_c);
  }
  if ((int)pppiStack_44 < 1) {
    if (iStack_40 < 1) goto LAB_004542c4;
    if ((int ****)pppiStack_44 == (int ****)0x0) {
      if (iStack_40 == 0) goto LAB_004542c4;
    }
    else if ((int)pppiStack_44 < 0) goto LAB_004542c4;
  }
  if (-1 < iStack_40) {
    FUN_0047a7f0(&pppiStack_44);
    piStack_c = &DAT_007af5a0;
    LOCK();
    DAT_007af5a0 = DAT_007af5a0 + 1;
    UNLOCK();
    pppiStack_68 = pppiStack_44;
    ppuVar15 = apuStack_64;
    apuStack_64[0] = apuStack_64[0] + (-0x14 - (int)pppiStack_44);
    uStack_14._0_1_ = 0xfa;
    FUN_005c3f10(&pppiStack_68);
    FUN_005c3f10(ppuVar15);
    FUN_004feac0(&piStack_c);
    piVar13 = piStack_c;
    uStack_14._0_1_ = 0xef;
    if (piStack_c != (int *)0x0) {
      LOCK();
      piStack_c = &DAT_007af5a0;
      UNLOCK();
      LOCK();
      iVar3 = *piVar13;
      *piVar13 = iVar3 + -1;
      UNLOCK();
      if ((iVar3 + -1 == 0) && (*piVar13 == 0)) {
        free(piVar13);
      }
    }
  }
LAB_004542c4:
  QString_fromAscii_maybe("mainwindow/maximized");
  uStack_34 = 0x80000000;
  auStack_3c[0] = 0;
  uStack_14._0_1_ = 0xfc;
  FUN_006514e0(auStack_2c,&piStack_c,auStack_3c);
  uStack_14._0_1_ = 0xfd;
  cVar2 = FUN_0063ace0();
  uStack_14._0_1_ = 0xfc;
  FUN_0063aa20();
  uStack_14._0_1_ = 0xfb;
  FUN_0063aa20();
  uStack_14 = CONCAT31(uStack_14._1_3_,0xef);
  LOCK();
  iVar3 = *piStack_c;
  *piStack_c = iVar3 + -1;
  UNLOCK();
  if (iVar3 + -1 == 0) {
    QString_freeData(piStack_c);
  }
  if (cVar2 == '\x01') {
    FUN_00478f60();
  }
  QString_fromAscii_maybe("sidebar/visible");
  uStack_34 = 0x80000000;
  auStack_3c[0] = 0;
  uStack_14 = CONCAT31(uStack_14._1_3_,0xff);
  FUN_006514e0(auStack_2c,&piStack_c,auStack_3c);
  uStack_14 = 0x100;
  iVar5 = FUN_0063c6c0(0);
  uStack_14 = 0xff;
  FUN_0063aa20();
  uStack_14._0_1_ = 0xfe;
  FUN_0063aa20();
  uStack_14 = CONCAT31(uStack_14._1_3_,0xef);
  LOCK();
  iVar3 = *piStack_c;
  *piStack_c = iVar3 + -1;
  UNLOCK();
  if (iVar3 + -1 == 0) {
    QString_freeData(piStack_c);
  }
  if (iVar5 == 1) {
    (*(code *)(*param_1[0x24])[0xb])(0);
    FUN_004885f0(0);
  }
  QString_fromAscii_maybe("toolbar/visible");
  uStack_34 = 0x80000000;
  auStack_3c[0] = 0;
  uStack_14._0_1_ = 2;
  uStack_14._1_3_ = 1;
  FUN_006514e0(auStack_2c,&piStack_c,auStack_3c);
  uStack_14._0_1_ = 3;
  iVar5 = FUN_0063c6c0(0);
  uStack_14._0_1_ = 2;
  FUN_0063aa20();
  uStack_14 = CONCAT31(uStack_14._1_3_,1);
  FUN_0063aa20();
  uStack_14 = 0xef;
  LOCK();
  iVar3 = *piStack_c;
  *piStack_c = iVar3 + -1;
  UNLOCK();
  if (iVar3 + -1 == 0) {
    QString_freeData(piStack_c);
  }
  if (iVar5 == 1) {
    (*(code *)(*param_1[0x10])[0xb])(0);
    FUN_004885f0(0);
  }
  QString_fromAscii_maybe("statusbar/visible");
  uStack_34 = 0x80000000;
  auStack_3c[0] = 0;
  uStack_14._0_1_ = 5;
  uStack_14._1_3_ = 1;
  FUN_006514e0(auStack_2c,&piStack_c,auStack_3c);
  uStack_14._0_1_ = 6;
  iVar5 = FUN_0063c6c0(0);
  uStack_14._0_1_ = 5;
  FUN_0063aa20();
  uStack_14 = CONCAT31(uStack_14._1_3_,4);
  FUN_0063aa20();
  uStack_14 = 0xef;
  LOCK();
  iVar3 = *piStack_c;
  *piStack_c = iVar3 + -1;
  UNLOCK();
  if (iVar3 + -1 == 0) {
    QString_freeData(piStack_c);
  }
  if (iVar5 == 1) {
    piVar13 = (int *)FUN_00484240();
    (**(code **)(*piVar13 + 0x2c))(0);
    FUN_004885f0(0);
  }
  QString_fromAscii_maybe("controls/mousesens");
  uStack_34 = 0x80000000;
  auStack_3c[0] = 0;
  uStack_14._0_1_ = 8;
  uStack_14._1_3_ = 1;
  FUN_006514e0(auStack_2c,&piStack_c,auStack_3c);
  uStack_14._0_1_ = 9;
  puVar12 = (undefined1 *)FUN_0063c6c0(0);
  uStack_14._0_1_ = 8;
  apuStack_64[0] = puVar12;
  FUN_0063aa20();
  uStack_14 = CONCAT31(uStack_14._1_3_,7);
  FUN_0063aa20();
  uStack_14 = 0xef;
  LOCK();
  iVar3 = *piStack_c;
  *piStack_c = iVar3 + -1;
  UNLOCK();
  if (iVar3 + -1 == 0) {
    QString_freeData(piStack_c);
  }
  if (0 < (int)puVar12) {
    FUN_004ff750(puVar12);
    param_1[9][0x26] = (int **)(float)(int)apuStack_64[0];
  }
  FUN_004504c0();
  QString_fromAscii_maybe("controls/speed");
  uStack_34 = 0x80000000;
  auStack_3c[0] = 0;
  uStack_14._0_1_ = 0xb;
  uStack_14._1_3_ = 1;
  FUN_006514e0(auStack_2c,&piStack_c,auStack_3c);
  uStack_14._0_1_ = 0xc;
  uVar8 = FUN_0063c6c0(0);
  uStack_14._0_1_ = 0xb;
  FUN_0063aa20();
  uStack_14 = CONCAT31(uStack_14._1_3_,10);
  FUN_0063aa20();
  uStack_14 = 0xef;
  LOCK();
  iVar3 = *piStack_c;
  *piStack_c = iVar3 + -1;
  UNLOCK();
  if (iVar3 + -1 == 0) {
    QString_freeData(piStack_c);
  }
  FUN_004ba8a0(uVar8);
  FUN_00458e80(uVar8);
  (*(code *)(*param_1)[0xb])(1);
  FUN_0044f980();
  uStack_18 = 0xe5;
  FUN_0064f6d0();
  uStack_18 = 0xe;
  LOCK();
  pppiVar4 = *unaff_EBP;
  *unaff_EBP = (int ***)((int)pppiVar4 + -1);
  UNLOCK();
  if ((int ***)((int)pppiVar4 + -1) == (int ***)0x0) {
    QString_freeData(unaff_EBP);
  }
  uStack_18 = 0;
  if (unaff_ESI != (int *)0x0) {
    LOCK();
    UNLOCK();
    LOCK();
    iVar3 = *unaff_ESI;
    *unaff_ESI = iVar3 + -1;
    UNLOCK();
    if ((iVar3 + -1 == 0) && (*unaff_ESI == 0)) {
      free(unaff_ESI);
    }
  }
  ExceptionList = pvStack_20;
  return param_1;
}


##### references to 006d6268
  from 00427c95 in FUN_00427c30 @ 00427c30
// ==== 00427c30 FUN_00427c30 size=771

void __fastcall FUN_00427c30(int param_1)

{
  int *piVar1;
  int iVar2;
  int *local_44;
  int *local_40;
  int *local_3c;
  int *local_38;
  int *local_34;
  int *local_30;
  undefined4 uStack_2c;
  undefined4 uStack_28;
  undefined4 uStack_24;
  undefined1 auStack_20 [8];
  undefined1 uStack_18;
  undefined4 uStack_14;
  undefined4 uStack_10;
  void *local_c;
  undefined1 *puStack_8;
  int local_4;
  
  local_4 = 0xffffffff;
  puStack_8 = &LAB_006a7ebb;
  local_c = ExceptionList;
  ExceptionList = &local_c;
  FUN_004e7a10();
  FUN_004f33c0(&local_44);
  local_4 = 0;
  if (local_44[3] == local_44[2] || local_44[3] - local_44[2] < 0) {
    FUN_0047c0f0(0);
    FUN_0047c0f0(0);
    FUN_0047c0f0(0);
    QString_fromAscii_maybe("Please select the IPL file you wish to add the new object to");
    local_4._0_1_ = 1;
    QString_fromAscii_maybe(&DAT_006d38a0);
    local_38 = &DAT_007af578;
    LOCK();
    UNLOCK();
    local_3c = &DAT_007af578;
    LOCK();
    UNLOCK();
    local_40 = &DAT_007af578;
    LOCK();
    DAT_007af578 = DAT_007af578 + 3;
    UNLOCK();
    local_4._0_1_ = 5;
    FUN_004abfe0(param_1,&local_34,&local_30,&local_40,&local_3c,&local_38,0,0xffffffff);
    local_4._0_1_ = 4;
    LOCK();
    iVar2 = *local_40;
    *local_40 = iVar2 + -1;
    UNLOCK();
    if (iVar2 + -1 == 0) {
      QString_freeData(local_40);
    }
    local_4._0_1_ = 3;
    LOCK();
    iVar2 = *local_3c;
    *local_3c = iVar2 + -1;
    UNLOCK();
    if (iVar2 + -1 == 0) {
      QString_freeData(local_3c);
    }
    local_4._0_1_ = 2;
    LOCK();
    iVar2 = *local_38;
    *local_38 = iVar2 + -1;
    UNLOCK();
    if (iVar2 + -1 == 0) {
      QString_freeData(local_38);
    }
    local_4._0_1_ = 1;
    LOCK();
    iVar2 = *local_34;
    *local_34 = iVar2 + -1;
    UNLOCK();
    if (iVar2 + -1 == 0) {
      QString_freeData(local_34);
    }
    local_4 = (uint)local_4._1_3_ << 8;
    LOCK();
    iVar2 = *local_30;
    *local_30 = iVar2 + -1;
    UNLOCK();
    if (iVar2 + -1 == 0) {
      QString_freeData(local_30);
    }
    piVar1 = local_44;
    local_4 = 0xffffffff;
    if (local_44 != (int *)0x0) {
      LOCK();
      local_44 = &DAT_007af5a0;
      UNLOCK();
      LOCK();
      iVar2 = *piVar1;
      *piVar1 = iVar2 + -1;
      UNLOCK();
      if ((iVar2 + -1 == 0) &&
         (FUN_00427290(piVar1 + piVar1[2] + 5,piVar1 + piVar1[3] + 5), *piVar1 == 0)) {
        free(piVar1);
        ExceptionList = local_c;
        return;
      }
    }
  }
  else {
    local_30 = (int *)FUN_006a3bdf(0x44);
    local_4._0_1_ = 6;
    if (local_30 == (int *)0x0) {
      piVar1 = (int *)0x0;
    }
    else {
      piVar1 = (int *)CIDEChooser__ctor_or_dtor_1(param_1,*(undefined4 *)(param_1 + 0x18));
    }
    local_4 = (uint)local_4._1_3_ << 8;
    iVar2 = FUN_004ac780();
    if (iVar2 == 1) {
      iVar2 = piVar1[0xe];
      (**(code **)(*piVar1 + 0xc))(1);
      Document_pushUndoSnapshot();
      FUN_00466d00(iVar2);
      local_30 = (int *)0xffffffff;
      uStack_2c = 0xffffffff;
      uStack_28 = 0;
      uStack_24 = 0;
      iVar2 = (*(int **)(param_1 + 0x40))[2];
      puStack_8 = (undefined1 *)CONCAT31(puStack_8._1_3_,7);
      (**(code **)(**(int **)(param_1 + 0x40) + 0x2c))
                (auStack_20,(*(int *)(iVar2 + 0xc) - *(int *)(iVar2 + 8)) + -1,0,&local_30);
      uStack_18 = 9;
      local_38 = (int *)0x0;
      local_34 = (int *)0x0;
      FUN_004daa70(&local_30);
      FUN_004779f0(7);
      (**(code **)(**(int **)(param_1 + 0x3c) + 0xe0))(&local_30,0);
      uStack_14 = 0;
      uStack_10 = 0;
    }
    piVar1 = local_44;
    local_4 = 0xffffffff;
    if (local_44 != (int *)0x0) {
      LOCK();
      local_44 = &DAT_007af5a0;
      UNLOCK();
      LOCK();
      iVar2 = *piVar1;
      *piVar1 = iVar2 + -1;
      UNLOCK();
      if ((iVar2 + -1 == 0) &&
         (FUN_00427290(piVar1 + piVar1[2] + 5,piVar1 + piVar1[3] + 5), *piVar1 == 0)) {
        free(piVar1);
      }
    }
  }
  ExceptionList = local_c;
  return;
}


##### references to 006d64b8
  from 0042b14e in CFileTree__ctor_or_dtor_CIPLTree_CTreeItem @ 0042a700
  from 0042b18c in CFileTree__ctor_or_dtor_CIPLTree_CTreeItem @ 0042a700
##### references to 006d55c0
  from 00421b6b in FUN_004218f0 @ 004218f0
// ==== 004218f0 FUN_004218f0 size=1103

void __fastcall FUN_004218f0(int param_1)

{
  int iVar1;
  void *pvVar2;
  bool bVar3;
  void **ppvVar4;
  int *piVar5;
  int *piVar6;
  int iVar7;
  int *piVar8;
  int *piVar9;
  int *local_30;
  int *local_2c;
  int *local_28;
  int *local_24;
  int *local_20;
  int *local_1c;
  int local_18;
  int *local_14;
  undefined4 local_10;
  void *local_c;
  undefined1 *puStack_8;
  int local_4;
  
  local_4 = 0xffffffff;
  puStack_8 = &LAB_006a7380;
  local_c = ExceptionList;
  local_30 = *(int **)(param_1 + 0x20);
  piVar9 = *(int **)(param_1 + 0x24);
  bVar3 = false;
  ExceptionList = &local_c;
  ppvVar4 = &local_c;
  local_18 = param_1;
  if ((int)local_30 < *(int *)(**(int **)(*(int *)(param_1 + 0x18) + 0xc) + 8)) {
LAB_00421936:
    do {
      piVar8 = local_30;
      piVar5 = *(int **)(*(int *)(param_1 + 0x18) + 0xc);
      piVar6 = (int *)*piVar5;
      local_2c = piVar9;
      if (*piVar6 != 1) {
        FUN_0058fc70(piVar6[2],piVar6[1]);
      }
      iVar1 = *(int *)(*(int *)(*piVar5 + 0x10 + (int)piVar8 * 4) + 0xc);
      if ((int)piVar9 < *(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8)) {
        FUN_004c9f80(&local_28);
        local_4 = 0;
        local_10 = FUN_00633500(&local_24);
        piVar5 = *(int **)(*(int *)(param_1 + 0x18) + 0xc);
        piVar6 = (int *)*piVar5;
        local_4 = CONCAT31(local_4._1_3_,1);
        if (*piVar6 != 1) {
          FUN_0058fc70(piVar6[2],piVar6[1]);
        }
        iVar1 = *(int *)(*piVar5 + 0x10 + (int)piVar8 * 4);
        piVar5 = *(int **)(iVar1 + 0xc);
        if (*piVar5 != 1) {
          local_14 = piVar5 + piVar5[2] + 5;
          piVar5 = (int *)FUN_00635250();
          if (piVar5 != (int *)0x0) {
            piVar9 = piVar5 + piVar5[3] + 5;
            piVar6 = piVar5 + piVar5[2] + 5;
            local_1c = piVar6;
            if (piVar6 != piVar9) {
              do {
                pvVar2 = (void *)piVar9[-1];
                piVar9 = piVar9 + -1;
                if (pvVar2 != (void *)0x0) {
                  LOCK();
                  iVar7 = **(int **)((int)pvVar2 + 8) + -1;
                  **(int **)((int)pvVar2 + 8) = iVar7;
                  UNLOCK();
                  if (iVar7 == 0) {
                    QString_freeData(*(undefined4 *)((int)pvVar2 + 8));
                  }
                  operator_delete(pvVar2);
                  piVar6 = local_1c;
                }
                param_1 = local_18;
              } while (piVar9 != piVar6);
            }
            piVar9 = local_2c;
            if (*piVar5 == 0) {
              free(piVar5);
              piVar9 = local_2c;
            }
          }
          iVar1 = *(int *)(iVar1 + 0xc);
          FUN_0040b900(iVar1 + 0x14 + *(int *)(iVar1 + 8) * 4,
                       iVar1 + 0x14 + *(int *)(iVar1 + 0xc) * 4,local_14);
        }
        FUN_00633500(&local_20);
        local_4._0_1_ = 2;
        iVar7 = FUN_0062fb50(local_10,0,1);
        local_4._0_1_ = 1;
        LOCK();
        iVar1 = *local_20;
        *local_20 = iVar1 + -1;
        UNLOCK();
        if (iVar1 + -1 == 0) {
          QString_freeData(local_20);
        }
        local_4 = (uint)local_4._1_3_ << 8;
        LOCK();
        iVar1 = *local_24;
        *local_24 = iVar1 + -1;
        UNLOCK();
        if (iVar1 + -1 == 0) {
          QString_freeData(local_24);
        }
        local_4 = 0xffffffff;
        LOCK();
        iVar1 = *local_28;
        *local_28 = iVar1 + -1;
        UNLOCK();
        if (iVar1 + -1 == 0) {
          QString_freeData(local_28);
        }
        piVar5 = local_30;
        if (iVar7 == -1) {
          piVar9 = (int *)((int)piVar9 + 1);
          goto LAB_00421936;
        }
        *(int **)(param_1 + 0x20) = local_30;
        *(int **)(param_1 + 0x24) = piVar9;
        piVar9 = *(int **)(*(int *)(param_1 + 0x18) + 0xc);
        piVar6 = (int *)*piVar9;
        if (*piVar6 != 1) {
          FUN_0058fc70(piVar6[2],piVar6[1]);
        }
        iVar1 = *(int *)(*(int *)(*piVar9 + 0x10 + (int)piVar5 * 4) + 0xc);
        piVar8 = (int *)(*(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8));
        bVar3 = true;
      }
      piVar9 = (int *)0x0;
      local_30 = (int *)((int)piVar8 + 1);
    } while ((int)local_30 < *(int *)(**(int **)(*(int *)(param_1 + 0x18) + 0xc) + 8));
    ppvVar4 = ExceptionList;
    if (bVar3) {
      QString_fromAscii_maybe("Find Next");
      local_4 = 3;
      FUN_0047e5a0(&local_2c);
      local_4 = 0xffffffff;
      LOCK();
      iVar1 = *local_2c;
      *local_2c = iVar1 + -1;
      UNLOCK();
      if (iVar1 + -1 == 0) {
        QString_freeData(local_2c);
      }
      FUN_004697b0(*(undefined4 *)(param_1 + 0x20),*(undefined4 *)(param_1 + 0x24));
      *(int *)(param_1 + 0x24) = *(int *)(param_1 + 0x24) + 1;
      ExceptionList = local_c;
      return;
    }
  }
  ExceptionList = ppvVar4;
  QString_fromAscii_maybe("Object Not Found");
  local_4 = 4;
  QString_fromAscii_maybe("Not Found");
  local_28 = &DAT_007af578;
  LOCK();
  UNLOCK();
  local_30 = &DAT_007af578;
  LOCK();
  UNLOCK();
  local_2c = &DAT_007af578;
  LOCK();
  DAT_007af578 = DAT_007af578 + 3;
  UNLOCK();
  local_4._0_1_ = 8;
  FUN_004abfa0(param_1,&local_24,&local_20,&local_2c,&local_30,&local_28,0,0xffffffff);
  local_4._0_1_ = 7;
  LOCK();
  iVar1 = *local_2c;
  *local_2c = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_2c);
  }
  local_4._0_1_ = 6;
  LOCK();
  iVar1 = *local_30;
  *local_30 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_30);
  }
  local_4._0_1_ = 5;
  LOCK();
  iVar1 = *local_28;
  *local_28 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_28);
  }
  local_4 = CONCAT31(local_4._1_3_,4);
  LOCK();
  iVar1 = *local_24;
  *local_24 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_24);
  }
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_20;
  *local_20 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_20);
  }
  QString_fromAscii_maybe(&DAT_006d5598);
  local_4 = 9;
  FUN_0047e5a0(&local_1c);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_1c;
  *local_1c = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_1c);
  }
  *(undefined4 *)(param_1 + 0x20) = 0;
  *(undefined4 *)(param_1 + 0x24) = 0;
  ExceptionList = local_c;
  return;
}


  from 004212d7 in FUN_00420db0 @ 00420db0
// ==== 00420db0 FUN_00420db0 size=1790

void __fastcall FUN_00420db0(int param_1)

{
  int iVar1;
  void *pvVar2;
  bool bVar3;
  char cVar4;
  int *piVar5;
  int *piVar6;
  undefined4 uVar7;
  int iVar8;
  int *piVar9;
  int iVar10;
  int *piVar11;
  int *local_28;
  int *local_24;
  int *local_20;
  int *local_1c;
  int *local_18;
  int *local_14;
  undefined4 local_10;
  void *local_c;
  undefined1 *puStack_8;
  int local_4;
  
  local_4 = 0xffffffff;
  puStack_8 = &LAB_006a7288;
  local_c = ExceptionList;
  bVar3 = false;
  ExceptionList = &local_c;
  cVar4 = FUN_0047e770();
  if (cVar4 != '\0') {
    piVar9 = *(int **)(param_1 + 0x24);
    piVar6 = *(int **)(param_1 + 0x28);
    iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 4);
    local_28 = piVar9;
    if ((int)piVar9 < *(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8)) {
      do {
        iVar1 = *(int *)(param_1 + 0x14);
        local_28 = piVar9;
        if (((**(int **)(iVar1 + 4) != 1) && (piVar5 = (int *)FUN_00635250(), piVar5 != (int *)0x0))
           && (*piVar5 == 0)) {
          free(piVar5);
        }
        iVar1 = *(int *)(iVar1 + 4);
        if (*(char *)(*(int *)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4) + 0x1a) ==
            '\0') {
          while( true ) {
            local_24 = piVar6;
            iVar1 = *(int *)(param_1 + 0x14);
            if (((**(int **)(iVar1 + 4) != 1) &&
                (piVar6 = (int *)FUN_00635250(), piVar6 != (int *)0x0)) && (*piVar6 == 0)) {
              free(piVar6);
            }
            iVar1 = *(int *)(iVar1 + 4);
            iVar1 = **(int **)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4);
            if (*(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8) <= (int)local_24) goto LAB_0042103c;
            FUN_004c9f80(&local_20);
            local_4 = 0;
            uVar7 = FUN_00633500(&local_1c);
            iVar1 = *(int *)(param_1 + 0x14);
            local_4 = CONCAT31(local_4._1_3_,1);
            if (((**(int **)(iVar1 + 4) != 1) &&
                (piVar6 = (int *)FUN_00635250(), piVar6 != (int *)0x0)) && (*piVar6 == 0)) {
              free(piVar6);
            }
            iVar1 = *(int *)(iVar1 + 4);
            piVar9 = *(int **)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4);
            piVar6 = (int *)*piVar9;
            if (*piVar6 != 1) {
              local_14 = piVar6 + piVar6[2] + 5;
              piVar6 = (int *)FUN_00635250();
              if ((piVar6 != (int *)0x0) &&
                 (FUN_0040b960(piVar6 + piVar6[2] + 5,piVar6 + piVar6[3] + 5), *piVar6 == 0)) {
                free(piVar6);
              }
              iVar1 = *piVar9;
              FUN_0040b6c0(iVar1 + 0x14 + *(int *)(iVar1 + 8) * 4,
                           iVar1 + 0x14 + *(int *)(iVar1 + 0xc) * 4,local_14);
            }
            piVar6 = local_24;
            FUN_00633500(&local_18);
            local_4._0_1_ = 2;
            iVar8 = FUN_0062fb50(uVar7,0,1);
            local_4._0_1_ = 1;
            LOCK();
            iVar1 = *local_18;
            *local_18 = iVar1 + -1;
            UNLOCK();
            if (iVar1 + -1 == 0) {
              QString_freeData(local_18);
            }
            local_4 = (uint)local_4._1_3_ << 8;
            LOCK();
            iVar1 = *local_1c;
            *local_1c = iVar1 + -1;
            UNLOCK();
            if (iVar1 + -1 == 0) {
              QString_freeData(local_1c);
            }
            local_4 = 0xffffffff;
            LOCK();
            iVar1 = *local_20;
            *local_20 = iVar1 + -1;
            UNLOCK();
            if (iVar1 + -1 == 0) {
              QString_freeData(local_20);
            }
            if (iVar8 != -1) break;
            piVar9 = local_28;
            piVar6 = (int *)((int)piVar6 + 1);
          }
          *(int **)(param_1 + 0x24) = local_28;
          *(int **)(param_1 + 0x28) = piVar6;
          if (((**(int **)(*(int *)(param_1 + 0x14) + 4) != 1) &&
              (piVar9 = (int *)FUN_00635250(), piVar9 != (int *)0x0)) && (*piVar9 == 0)) {
            free(piVar9);
          }
          iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 4);
          piVar9 = (int *)(*(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8));
          bVar3 = true;
LAB_0042103c:
          piVar6 = (int *)0x0;
        }
        iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 4);
        piVar9 = (int *)((int)piVar9 + 1);
        local_28 = piVar9;
      } while ((int)piVar9 < *(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8));
    }
  }
  cVar4 = FUN_0047e770();
  if (cVar4 != '\0') {
    piVar9 = *(int **)(param_1 + 0x24);
    piVar6 = *(int **)(param_1 + 0x28);
    iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 4);
    local_28 = piVar9;
    if ((int)piVar9 < *(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8)) {
      do {
        iVar1 = *(int *)(param_1 + 0x14);
        local_28 = piVar9;
        if (((**(int **)(iVar1 + 4) != 1) && (piVar5 = (int *)FUN_00635250(), piVar5 != (int *)0x0))
           && (*piVar5 == 0)) {
          free(piVar5);
        }
        iVar1 = *(int *)(iVar1 + 4);
        if (*(char *)(*(int *)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4) + 0x1a) ==
            '\0') {
          while( true ) {
            local_20 = piVar6;
            iVar1 = *(int *)(param_1 + 0x14);
            if (((**(int **)(iVar1 + 4) != 1) &&
                (piVar6 = (int *)FUN_00635250(), piVar6 != (int *)0x0)) && (*piVar6 == 0)) {
              free(piVar6);
            }
            iVar1 = *(int *)(iVar1 + 4);
            iVar1 = **(int **)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4);
            if (*(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8) <= (int)local_20) goto LAB_004212b0;
            local_10 = FUN_004c9f80(&local_18);
            iVar1 = *(int *)(param_1 + 0x14);
            local_4 = 3;
            if (((**(int **)(iVar1 + 4) != 1) &&
                (piVar9 = (int *)FUN_00635250(), piVar9 != (int *)0x0)) && (*piVar9 == 0)) {
              free(piVar9);
            }
            iVar1 = *(int *)(iVar1 + 4);
            piVar9 = *(int **)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)local_28) * 4);
            piVar6 = (int *)*piVar9;
            if (*piVar6 != 1) {
              local_1c = piVar6 + piVar6[2] + 5;
              piVar6 = (int *)FUN_00635250();
              local_24 = piVar6;
              if (piVar6 != (int *)0x0) {
                piVar5 = piVar6 + piVar6[3] + 5;
                piVar11 = piVar6 + piVar6[2] + 5;
                local_14 = piVar11;
                if (piVar11 != piVar5) {
                  do {
                    pvVar2 = (void *)piVar5[-1];
                    piVar5 = piVar5 + -1;
                    if (pvVar2 != (void *)0x0) {
                      LOCK();
                      iVar1 = **(int **)((int)pvVar2 + 8) + -1;
                      **(int **)((int)pvVar2 + 8) = iVar1;
                      UNLOCK();
                      if (iVar1 == 0) {
                        QString_freeData(*(undefined4 *)((int)pvVar2 + 8));
                      }
                      operator_delete(pvVar2);
                      piVar6 = local_24;
                      piVar11 = local_14;
                    }
                  } while (piVar5 != piVar11);
                }
                if (*piVar6 == 0) {
                  free(local_24);
                }
              }
              iVar1 = *piVar9;
              FUN_0040b6c0(iVar1 + 0x14 + *(int *)(iVar1 + 8) * 4,
                           iVar1 + 0x14 + *(int *)(iVar1 + 0xc) * 4,local_1c);
            }
            piVar6 = local_20;
            iVar1 = *piVar9;
            iVar8 = *(int *)(iVar1 + 8) + (int)local_20;
            iVar10 = QString_toInt(0,10);
            iVar8 = *(int *)(*(int *)(iVar1 + 0x14 + iVar8 * 4) + 4);
            local_4 = 0xffffffff;
            LOCK();
            iVar1 = *local_18;
            *local_18 = iVar1 + -1;
            UNLOCK();
            if (iVar1 + -1 == 0) {
              QString_freeData(local_18);
            }
            if (iVar8 == iVar10) break;
            piVar9 = local_28;
            piVar6 = (int *)((int)piVar6 + 1);
          }
          *(int **)(param_1 + 0x24) = local_28;
          *(int **)(param_1 + 0x28) = piVar6;
          if (((**(int **)(*(int *)(param_1 + 0x14) + 4) != 1) &&
              (piVar9 = (int *)FUN_00635250(), piVar9 != (int *)0x0)) && (*piVar9 == 0)) {
            free(piVar9);
          }
          iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 4);
          piVar9 = (int *)(*(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8));
          bVar3 = true;
LAB_004212b0:
          piVar6 = (int *)0x0;
        }
        iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 4);
        piVar9 = (int *)((int)piVar9 + 1);
        local_28 = piVar9;
      } while ((int)piVar9 < *(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8));
    }
  }
  if (!bVar3) {
    QString_fromAscii_maybe("Item Placement Not Found");
    local_4 = 5;
    QString_fromAscii_maybe("Not Found");
    local_28 = &DAT_007af578;
    LOCK();
    UNLOCK();
    local_24 = &DAT_007af578;
    LOCK();
    UNLOCK();
    local_20 = &DAT_007af578;
    LOCK();
    DAT_007af578 = DAT_007af578 + 3;
    UNLOCK();
    local_4._0_1_ = 9;
    FUN_004abfa0(param_1,&local_1c,&local_18,&local_20,&local_24,&local_28,0,0xffffffff);
    local_4._0_1_ = 8;
    LOCK();
    iVar1 = *local_20;
    *local_20 = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(local_20);
    }
    local_4._0_1_ = 7;
    LOCK();
    iVar1 = *local_24;
    *local_24 = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(local_24);
    }
    local_4._0_1_ = 6;
    LOCK();
    iVar1 = *local_28;
    *local_28 = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(local_28);
    }
    local_4 = CONCAT31(local_4._1_3_,5);
    LOCK();
    iVar1 = *local_1c;
    *local_1c = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(local_1c);
    }
    local_4 = 0xffffffff;
    LOCK();
    iVar1 = *local_18;
    *local_18 = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(local_18);
    }
    QString_fromAscii_maybe(&DAT_006d5598);
    local_4 = 10;
    FUN_0047e5a0(&local_14);
    local_4 = 0xffffffff;
    LOCK();
    iVar1 = *local_14;
    *local_14 = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(local_14);
    }
    *(undefined4 *)(param_1 + 0x24) = 0;
    *(undefined4 *)(param_1 + 0x28) = 0;
    ExceptionList = local_c;
    return;
  }
  QString_fromAscii_maybe("Find Next");
  local_4 = 4;
  FUN_0047e5a0(&local_20);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_20;
  *local_20 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_20);
  }
  FUN_00466900(*(undefined4 *)(param_1 + 0x24),*(undefined4 *)(param_1 + 0x28));
  *(int *)(param_1 + 0x28) = *(int *)(param_1 + 0x28) + 1;
  ExceptionList = local_c;
  return;
}


  from 00420566 in FUN_0041fdf0 @ 0041fdf0
// ==== 0041fdf0 FUN_0041fdf0 size=2380

void __fastcall FUN_0041fdf0(int param_1)

{
  int iVar1;
  void *pvVar2;
  bool bVar3;
  char cVar4;
  int *piVar5;
  int *piVar6;
  undefined4 uVar7;
  int iVar8;
  int *piVar9;
  int iVar10;
  int *local_28;
  int *local_24;
  int *local_20;
  int *local_1c;
  int *local_18;
  int *local_14;
  int *local_10;
  void *local_c;
  undefined1 *puStack_8;
  int local_4;
  
  local_4 = 0xffffffff;
  puStack_8 = &LAB_006a7150;
  local_c = ExceptionList;
  bVar3 = false;
  ExceptionList = &local_c;
  cVar4 = FUN_0047e770();
  if (cVar4 != '\0') {
    piVar9 = *(int **)(param_1 + 0x28);
    piVar6 = *(int **)(param_1 + 0x2c);
    iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 8);
    local_28 = piVar9;
    if ((int)piVar9 < *(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8)) {
      do {
        iVar1 = *(int *)(param_1 + 0x14);
        local_28 = piVar9;
        if (((**(int **)(iVar1 + 8) != 1) && (piVar5 = (int *)FUN_00635250(), piVar5 != (int *)0x0))
           && (*piVar5 == 0)) {
          free(piVar5);
        }
        iVar1 = *(int *)(iVar1 + 8);
        if (*(char *)(*(int *)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4) + 0x1d) ==
            '\0') {
          while( true ) {
            local_24 = piVar6;
            iVar1 = *(int *)(param_1 + 0x14);
            if (((**(int **)(iVar1 + 8) != 1) &&
                (piVar6 = (int *)FUN_00635250(), piVar6 != (int *)0x0)) && (*piVar6 == 0)) {
              free(piVar6);
            }
            iVar1 = *(int *)(iVar1 + 8);
            iVar1 = **(int **)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4);
            if (*(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8) <= (int)local_24) goto LAB_0042007d;
            FUN_004c9f80(&local_20);
            local_4 = 0;
            uVar7 = FUN_00633500(&local_1c);
            iVar1 = *(int *)(param_1 + 0x14);
            local_4 = CONCAT31(local_4._1_3_,1);
            if (((**(int **)(iVar1 + 8) != 1) &&
                (piVar6 = (int *)FUN_00635250(), piVar6 != (int *)0x0)) && (*piVar6 == 0)) {
              free(piVar6);
            }
            iVar1 = *(int *)(iVar1 + 8);
            piVar9 = *(int **)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4);
            piVar6 = (int *)*piVar9;
            if (*piVar6 != 1) {
              local_14 = piVar6 + piVar6[2] + 5;
              piVar6 = (int *)FUN_00635250();
              if ((piVar6 != (int *)0x0) &&
                 (FUN_0040ba40(piVar6 + piVar6[2] + 5,piVar6 + piVar6[3] + 5), *piVar6 == 0)) {
                free(piVar6);
              }
              iVar1 = *piVar9;
              FUN_0040ba80(iVar1 + 0x14 + *(int *)(iVar1 + 8) * 4,
                           iVar1 + 0x14 + *(int *)(iVar1 + 0xc) * 4,local_14);
            }
            piVar6 = local_24;
            FUN_00633500(&local_18);
            local_4._0_1_ = 2;
            iVar8 = FUN_0062fb50(uVar7,0,1);
            local_4._0_1_ = 1;
            LOCK();
            iVar1 = *local_18;
            *local_18 = iVar1 + -1;
            UNLOCK();
            if (iVar1 + -1 == 0) {
              QString_freeData(local_18);
            }
            local_4 = (uint)local_4._1_3_ << 8;
            LOCK();
            iVar1 = *local_1c;
            *local_1c = iVar1 + -1;
            UNLOCK();
            if (iVar1 + -1 == 0) {
              QString_freeData(local_1c);
            }
            local_4 = 0xffffffff;
            LOCK();
            iVar1 = *local_20;
            *local_20 = iVar1 + -1;
            UNLOCK();
            if (iVar1 + -1 == 0) {
              QString_freeData(local_20);
            }
            if (iVar8 != -1) break;
            piVar9 = local_28;
            piVar6 = (int *)((int)piVar6 + 1);
          }
          *(int **)(param_1 + 0x28) = local_28;
          *(int **)(param_1 + 0x2c) = piVar6;
          if (((**(int **)(*(int *)(param_1 + 0x14) + 8) != 1) &&
              (piVar9 = (int *)FUN_00635250(), piVar9 != (int *)0x0)) && (*piVar9 == 0)) {
            free(piVar9);
          }
          iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 8);
          piVar9 = (int *)(*(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8));
          bVar3 = true;
LAB_0042007d:
          piVar6 = (int *)0x0;
        }
        iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 8);
        piVar9 = (int *)((int)piVar9 + 1);
        local_28 = piVar9;
      } while ((int)piVar9 < *(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8));
    }
  }
  cVar4 = FUN_0047e770();
  if (cVar4 != '\0') {
    piVar9 = *(int **)(param_1 + 0x28);
    piVar6 = *(int **)(param_1 + 0x2c);
    iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 8);
    local_28 = piVar9;
    if ((int)piVar9 < *(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8)) {
      do {
        iVar1 = *(int *)(param_1 + 0x14);
        local_28 = piVar9;
        if (((**(int **)(iVar1 + 8) != 1) && (piVar5 = (int *)FUN_00635250(), piVar5 != (int *)0x0))
           && (*piVar5 == 0)) {
          free(piVar5);
        }
        iVar1 = *(int *)(iVar1 + 8);
        if (*(char *)(*(int *)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4) + 0x1d) ==
            '\0') {
          while( true ) {
            local_24 = piVar6;
            iVar1 = *(int *)(param_1 + 0x14);
            if (((**(int **)(iVar1 + 8) != 1) &&
                (piVar6 = (int *)FUN_00635250(), piVar6 != (int *)0x0)) && (*piVar6 == 0)) {
              free(piVar6);
            }
            iVar1 = *(int *)(iVar1 + 8);
            iVar1 = **(int **)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4);
            if (*(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8) <= (int)local_24) goto LAB_004202dd;
            local_10 = (int *)FUN_004c9f80(&local_18);
            iVar1 = *(int *)(param_1 + 0x14);
            local_4 = 3;
            if (((**(int **)(iVar1 + 8) != 1) &&
                (piVar6 = (int *)FUN_00635250(), piVar6 != (int *)0x0)) && (*piVar6 == 0)) {
              free(piVar6);
            }
            iVar1 = *(int *)(iVar1 + 8);
            piVar9 = *(int **)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4);
            piVar6 = (int *)*piVar9;
            if (*piVar6 != 1) {
              local_20 = piVar6 + piVar6[2] + 5;
              local_14 = (int *)FUN_00635250();
              if (local_14 != (int *)0x0) {
                piVar6 = local_14 + local_14[3] + 5;
                local_1c = local_14 + local_14[2] + 5;
                if (local_1c != piVar6) {
                  do {
                    pvVar2 = (void *)piVar6[-1];
                    piVar6 = piVar6 + -1;
                    if (pvVar2 != (void *)0x0) {
                      FUN_0040b740();
                      operator_delete(pvVar2);
                    }
                  } while (piVar6 != local_1c);
                }
                if (*local_14 == 0) {
                  free(local_14);
                }
              }
              iVar1 = *piVar9;
              FUN_0040ba80(iVar1 + 0x14 + *(int *)(iVar1 + 8) * 4,
                           iVar1 + 0x14 + *(int *)(iVar1 + 0xc) * 4,local_20);
            }
            piVar6 = local_24;
            iVar8 = FUN_0062fb50(local_10,0,1);
            local_4 = 0xffffffff;
            LOCK();
            iVar1 = *local_18;
            *local_18 = iVar1 + -1;
            UNLOCK();
            if (iVar1 + -1 == 0) {
              QString_freeData(local_18);
            }
            if (iVar8 != -1) break;
            piVar9 = local_28;
            piVar6 = (int *)((int)piVar6 + 1);
          }
          *(int **)(param_1 + 0x28) = local_28;
          *(int **)(param_1 + 0x2c) = piVar6;
          if (((**(int **)(*(int *)(param_1 + 0x14) + 8) != 1) &&
              (piVar9 = (int *)FUN_00635250(), piVar9 != (int *)0x0)) && (*piVar9 == 0)) {
            free(piVar9);
          }
          iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 8);
          piVar9 = (int *)(*(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8));
          bVar3 = true;
LAB_004202dd:
          piVar6 = (int *)0x0;
        }
        iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 8);
        piVar9 = (int *)((int)piVar9 + 1);
        local_28 = piVar9;
      } while ((int)piVar9 < *(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8));
    }
  }
  cVar4 = FUN_0047e770();
  if (cVar4 != '\0') {
    piVar9 = *(int **)(param_1 + 0x28);
    piVar6 = *(int **)(param_1 + 0x2c);
    iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 8);
    local_24 = piVar9;
    if ((int)piVar9 < *(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8)) {
      do {
        iVar1 = *(int *)(param_1 + 0x14);
        local_24 = piVar9;
        if (((**(int **)(iVar1 + 8) != 1) && (piVar5 = (int *)FUN_00635250(), piVar5 != (int *)0x0))
           && (*piVar5 == 0)) {
          free(piVar5);
        }
        iVar1 = *(int *)(iVar1 + 8);
        if (*(char *)(*(int *)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4) + 0x1d) ==
            '\0') {
          while( true ) {
            local_28 = piVar6;
            iVar1 = *(int *)(param_1 + 0x14);
            if (((**(int **)(iVar1 + 8) != 1) &&
                (piVar6 = (int *)FUN_00635250(), piVar6 != (int *)0x0)) && (*piVar6 == 0)) {
              free(piVar6);
            }
            iVar1 = *(int *)(iVar1 + 8);
            iVar1 = **(int **)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4);
            if (*(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8) <= (int)local_28) goto LAB_0042053f;
            local_20 = (int *)FUN_004c9f80(&local_18);
            iVar1 = *(int *)(param_1 + 0x14);
            local_4 = 4;
            if (((**(int **)(iVar1 + 8) != 1) &&
                (piVar6 = (int *)FUN_00635250(), piVar6 != (int *)0x0)) && (*piVar6 == 0)) {
              free(piVar6);
            }
            iVar1 = *(int *)(iVar1 + 8);
            piVar9 = *(int **)(iVar1 + 0x14 + (*(int *)(iVar1 + 8) + (int)piVar9) * 4);
            piVar6 = (int *)*piVar9;
            if (*piVar6 != 1) {
              local_1c = piVar6 + piVar6[2] + 5;
              local_14 = (int *)FUN_00635250();
              if (local_14 != (int *)0x0) {
                piVar6 = local_14 + local_14[3] + 5;
                local_10 = local_14 + local_14[2] + 5;
                if (local_10 != piVar6) {
                  do {
                    pvVar2 = (void *)piVar6[-1];
                    piVar6 = piVar6 + -1;
                    if (pvVar2 != (void *)0x0) {
                      FUN_0040b740();
                      operator_delete(pvVar2);
                    }
                  } while (piVar6 != local_10);
                }
                if (*local_14 == 0) {
                  free(local_14);
                }
              }
              iVar1 = *piVar9;
              FUN_0040ba80(iVar1 + 0x14 + *(int *)(iVar1 + 8) * 4,
                           iVar1 + 0x14 + *(int *)(iVar1 + 0xc) * 4,local_1c);
            }
            iVar1 = *piVar9;
            iVar8 = *(int *)(iVar1 + 8) + (int)local_28;
            iVar10 = QString_toInt(0,10);
            iVar8 = *(int *)(*(int *)(iVar1 + 0x14 + iVar8 * 4) + 0x5c);
            local_4 = 0xffffffff;
            LOCK();
            iVar1 = *local_18;
            *local_18 = iVar1 + -1;
            UNLOCK();
            if (iVar1 + -1 == 0) {
              QString_freeData(local_18);
            }
            if (iVar8 == iVar10) break;
            piVar9 = local_24;
            piVar6 = (int *)((int)local_28 + 1);
          }
          *(int **)(param_1 + 0x2c) = local_28;
          *(int **)(param_1 + 0x28) = local_24;
          if (((**(int **)(*(int *)(param_1 + 0x14) + 8) != 1) &&
              (piVar9 = (int *)FUN_00635250(), piVar9 != (int *)0x0)) && (*piVar9 == 0)) {
            free(piVar9);
          }
          iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 8);
          piVar9 = (int *)(*(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8));
          bVar3 = true;
LAB_0042053f:
          piVar6 = (int *)0x0;
        }
        iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 8);
        piVar9 = (int *)((int)piVar9 + 1);
        local_24 = piVar9;
      } while ((int)piVar9 < *(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 8));
    }
  }
  if (!bVar3) {
    QString_fromAscii_maybe("Object Not Found");
    local_4 = 6;
    QString_fromAscii_maybe("Not Found");
    local_20 = &DAT_007af578;
    LOCK();
    UNLOCK();
    local_28 = &DAT_007af578;
    LOCK();
    UNLOCK();
    local_24 = &DAT_007af578;
    LOCK();
    DAT_007af578 = DAT_007af578 + 3;
    UNLOCK();
    local_4._0_1_ = 10;
    FUN_004abfa0(param_1,&local_1c,&local_18,&local_24,&local_28,&local_20,0,0xffffffff);
    local_4._0_1_ = 9;
    LOCK();
    iVar1 = *local_24;
    *local_24 = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(local_24);
    }
    local_4._0_1_ = 8;
    LOCK();
    iVar1 = *local_28;
    *local_28 = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(local_28);
    }
    local_4._0_1_ = 7;
    LOCK();
    iVar1 = *local_20;
    *local_20 = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(local_20);
    }
    local_4 = CONCAT31(local_4._1_3_,6);
    LOCK();
    iVar1 = *local_1c;
    *local_1c = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(local_1c);
    }
    local_4 = 0xffffffff;
    LOCK();
    iVar1 = *local_18;
    *local_18 = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(local_18);
    }
    QString_fromAscii_maybe(&DAT_006d5598);
    local_4 = 0xb;
    FUN_0047e5a0(&local_14);
    local_4 = 0xffffffff;
    LOCK();
    iVar1 = *local_14;
    *local_14 = iVar1 + -1;
    UNLOCK();
    if (iVar1 + -1 == 0) {
      QString_freeData(local_14);
    }
    *(undefined4 *)(param_1 + 0x28) = 0;
    *(undefined4 *)(param_1 + 0x2c) = 0;
    ExceptionList = local_c;
    return;
  }
  QString_fromAscii_maybe("Find Next");
  local_4 = 5;
  FUN_0047e5a0(&local_24);
  local_4 = 0xffffffff;
  LOCK();
  iVar1 = *local_24;
  *local_24 = iVar1 + -1;
  UNLOCK();
  if (iVar1 + -1 == 0) {
    QString_freeData(local_24);
  }
  FUN_004667b0(*(undefined4 *)(param_1 + 0x28),*(undefined4 *)(param_1 + 0x2c));
  *(int *)(param_1 + 0x2c) = *(int *)(param_1 + 0x2c) + 1;
  ExceptionList = local_c;
  return;
}


