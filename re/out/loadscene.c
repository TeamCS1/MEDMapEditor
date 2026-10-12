// ==== 0040e720 FUN_0040e720 size=9392
/* WARNING: Removing unreachable block (ram,0x0040ff29) */
/* WARNING: Removing unreachable block (ram,0x0040ff3c) */
/* WARNING: Heritage AFTER dead removal. Example location: r0x007af5a0 : 0x004106c2 */
/* WARNING: Restarted to delay deadcode elimination for space: ram */
undefined4 __fastcall FUN_0040e720(int param_1)
{
  int *piVar1;
  void *pvVar2;
  bool bVar3;
  bool bVar4;
  undefined1 uVar5;
  char cVar6;
  undefined4 uVar7;
  int iVar8;
  int *piVar9;
  int *piVar10;
  int *piVar11;
  undefined4 *puVar12;
  int iVar13;
  int *piVar14;
  int iVar15;
  int iVar16;
  int *piVar17;
  undefined4 uStack_7c;
  int *local_6c;
  int *local_60;
  int *local_5c;
  int *local_58;
  int *local_50;
  int *local_4c;
  int *local_48;
  int *local_44;
  int *local_40;
  int *local_3c;
  int *local_38;
  int *local_34;
  int *local_30;
  int *local_2c;
  int *piStack_28;
  int *local_24;
  int *local_20;
  void *local_14;
  void *pvStack_c;
  undefined1 *puStack_8;
  undefined4 local_4;
  local_4 = 0xffffffff;
  puStack_8 = &LAB_006a5e72;
  pvStack_c = ExceptionList;
  bVar4 = false;
  local_20 = (int *)FUN_006a3bdf();
  local_4 = 0;
  if (local_20 == (int *)0x0) {
    local_60 = (int *)0x0;
  }
  else {
    local_60 = (int *)ProgressDialog__ctor_or_dtor();
  }
  piVar1 = *(int **)(param_1 + 0x1c);
  puVar12 = (undefined4 *)(param_1 + 0x1c);
  *piVar1 = *piVar1 + 1;
  local_4 = 1;
  QString_assignLiteral_maybe();
  local_20 = (int *)&stack0xffffff70;
  *piVar1 = *piVar1 + 1;
  FUN_0041f2c0();
  pvVar2 = *(void **)(param_1 + 0x14);
  if (pvVar2 != (void *)0x0) {
    FUN_0040c970();
    operator_delete(pvVar2);
  }
  local_20 = (int *)FUN_006a3bdf();
  if (local_20 == (int *)0x0) {
    uVar7 = 0;
  }
  else {
    local_24 = (int *)&stack0xffffff6c;
    *(int *)*puVar12 = *(int *)*puVar12 + 1;
    uVar7 = FUN_00462ed0();
  }
  *(undefined4 *)(param_1 + 0x14) = uVar7;
  FUN_00630420();
  if (*(char *)(param_1 + 0x10) == '\x01') {
    QString_assignLiteral_maybe();
  }
  if (*(char *)(param_1 + 0x10) == '\x02') {
    QString_assignLiteral_maybe();
  }
  if (*(char *)(param_1 + 0x10) == '\x03') {
    QString_assignLiteral_maybe();
  }
  FUN_00636320();
  if (*(char *)(param_1 + 0x10) == '\x01') {
    local_58 = (int *)*puVar12;
    *local_58 = *local_58 + 1;
    QString_assignLiteral_maybe();
    local_20 = (int *)&stack0xffffff70;
    *local_58 = *local_58 + 1;
    FUN_0041f2c0();
    if (iVar16 + -1 == 0) {
      QString_freeData();
    }
  }
  if (*(char *)(param_1 + 0x10) == '\x03') {
    local_6c = (int *)*puVar12;
    *local_6c = *local_6c + 1;
    QString_assignLiteral_maybe();
    local_20 = (int *)&stack0xffffff70;
    *local_6c = *local_6c + 1;
    FUN_0041f2c0();
    FUN_00630420();
    QString_assignLiteral_maybe();
    local_20 = (int *)&stack0xffffff70;
    *local_6c = *local_6c + 1;
    FUN_0041f2c0();
    FUN_00630420();
    QString_assignLiteral_maybe();
    local_20 = (int *)&stack0xffffff70;
    *local_6c = *local_6c + 1;
    FUN_0041f2c0();
    if (iVar16 + -1 == 0) {
      QString_freeData();
    }
  }
  FUN_006411c0();
  local_20 = (int *)&stack0xffffff70;
  cVar6 = FUN_00641490();
  if (cVar6 == '\0') {
    QString_fromAscii_maybe();
    QString_fromAscii_maybe();
    local_34 = &DAT_007af578;
    local_30 = &DAT_007af578;
    local_2c = &DAT_007af578;
    DAT_007af578 = DAT_007af578 + 3;
    FUN_004abfe0(0,&local_38,&local_3c);
    if (iVar16 + -1 == 0) {
      QString_freeData();
    }
    if (iVar16 + -1 == 0) {
      QString_freeData();
    }
    if (iVar16 + -1 == 0) {
      QString_freeData();
    }
    if (iVar16 + -1 == 0) {
      QString_freeData();
    }
    if (iVar16 + -1 == 0) {
      QString_freeData();
    }
LAB_00410b8b:
    FUN_00641a10();
    FUN_006361b0();
    iVar16 = *piVar1 + -1;
    *piVar1 = iVar16;
joined_r0x0040f83a:
    if (iVar16 == 0) {
LAB_00410bca:
      local_4 = 0xffffffff;
      QString_freeData();
    }
    return 0;
  }
  FUN_00643770();
  FUN_00643890();
  FUN_00630420();
  if (iVar16 + -1 == 0) {
    QString_freeData();
  }
  FUN_00630420();
  FUN_00641650();
  QString_fromAscii_maybe();
  FUN_00634320();
  if (iVar16 + -1 == 0) {
    QString_freeData();
  }
  iVar16 = 0;
  if (local_6c[3] != local_6c[2] && -1 < local_6c[3] - local_6c[2]) {
    do {
      piVar17 = (int *)local_6c[local_6c[2] + iVar16 + 5];
      *piVar17 = *piVar17 + 1;
      QString_fromAscii_maybe();
      iVar8 = FUN_0062fb50();
      if (iVar8 == -1) {
        QString_fromAscii_maybe();
        bVar4 = true;
        local_4 = 0xe;
        iVar8 = FUN_0062fb50();
        uStack_7c = (int *)CONCAT13(1,(undefined3)uStack_7c);
        if (iVar8 == -1) goto LAB_0040eb6d;
      }
      else {
LAB_0040eb6d:
        uStack_7c = (int *)((uint)uStack_7c & 0xffffff);
      }
      local_4 = 0xd;
      if (bVar4) {
        bVar4 = false;
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
      }
      if (iVar8 + -1 == 0) {
        QString_freeData();
      }
      if (uStack_7c._3_1_ != '\0') {
        QString_fromAscii_maybe();
        FUN_00630ad0();
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
        QString_fromAscii_maybe();
        FUN_00630ad0();
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
        QString_fromAscii_maybe();
        FUN_00630ad0();
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
        QString_fromAscii_maybe();
        QString_fromAscii_maybe();
        FUN_00630d70();
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
        local_5c = *(int **)(param_1 + 0x1c);
        *local_5c = *local_5c + 1;
        QString_assignLiteral_maybe();
        FUN_00630470();
        local_20 = (int *)&stack0xffffff70;
        *local_5c = *local_5c + 1;
        FUN_0041f2c0();
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
      }
      if (iVar8 + -1 == 0) {
        QString_freeData();
      }
      iVar16 = iVar16 + 1;
    } while (iVar16 < local_6c[3] - local_6c[2]);
  }
  bVar4 = false;
  QString_fromAscii_maybe();
  FUN_004b7c50();
  if (iVar16 + -1 == 0) {
    QString_freeData();
  }
  FUN_004b7530();
  FUN_004b7510();
  (**(code **)(*local_60 + 0x2c))();
  local_20 = (int *)&stack0xffffff70;
  FUN_00643bd0();
  local_20 = (int *)thunk_FUN_006a3bdf();
  if (local_20 == (int *)0x0) {
    local_58 = (int *)0x0;
  }
  else {
    local_58 = local_20 + 1;
    *local_20 = 30000;
    _eh_vector_constructor_iterator_(local_58,8,30000,FUN_0040b630,QString_destroy_maybe);
  }
  local_44 = (int *)0x0;
  if (0 < local_6c[3] - local_6c[2]) {
    do {
      piVar17 = (int *)local_6c[(int)local_44 + local_6c[2] + 5];
      *piVar17 = *piVar17 + 1;
      QString_fromAscii_maybe();
      iVar16 = FUN_0062fb50();
      if (iVar16 == -1) {
        QString_fromAscii_maybe();
        bVar4 = true;
        local_4 = 0x19;
        iVar16 = FUN_0062fb50();
        if (iVar16 == -1) goto LAB_0040ef5f;
        bVar3 = true;
      }
      else {
LAB_0040ef5f:
        bVar3 = false;
      }
      local_4 = 0x18;
      if (bVar4) {
        bVar4 = false;
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
      }
      if (iVar16 + -1 == 0) {
        QString_freeData();
      }
      if (bVar3) {
        QString_fromAscii_maybe();
        FUN_00636320();
        FUN_00635bc0();
        FUN_00630470();
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
        FUN_006361b0();
        FUN_004b7c50();
        local_30 = (int *)(local_6c[3] - local_6c[2]);
        FUN_006a3cb0();
        FUN_004b7ff0();
        local_20 = (int *)&stack0xffffff70;
        FUN_00643bd0();
        cVar6 = FUN_004b74f0();
        if (cVar6 != '\0') {
          if (iVar16 + -1 == 0) {
            QString_freeData();
          }
          if (iVar16 + -1 == 0) {
            QString_freeData();
          }
          if (local_6c != (int *)0x0) {
            if ((iVar16 + -1 == 0) && (FUN_0040a110(), *local_6c == 0)) {
              free(local_6c);
            }
          }
          FUN_00642f50();
          FUN_00641a10();
          FUN_006361b0();
          iVar16 = *piVar1 + -1;
          *piVar1 = iVar16;
          goto joined_r0x0040f83a;
        }
        QString_fromAscii_maybe();
        FUN_00630ad0();
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
        local_20 = (int *)FUN_006a3bdf();
        if (local_20 == (int *)0x0) {
          piVar9 = (int *)0x0;
        }
        else {
          *piVar17 = *piVar17 + 1;
          local_24 = (int *)&stack0xffffff60;
          **(int **)(param_1 + 0x1c) = **(int **)(param_1 + 0x1c) + 1;
          piVar9 = (int *)FUN_00417610();
        }
        FUN_00418230();
        piVar10 = (int *)*piVar9;
        iVar16 = piVar10[2];
        local_50 = (int *)0x0;
        if (piVar10[3] != iVar16 && -1 < piVar10[3] - iVar16) {
          do {
            if (*piVar10 != 1) {
              piStack_28 = piVar10 + iVar16 + 5;
              local_30 = (int *)FUN_00635250();
              if (local_30 != (int *)0x0) {
                piVar10 = local_30 + local_30[3] + 5;
                piVar14 = local_30 + local_30[2] + 5;
                while (piVar14 != piVar10) {
                  pvVar2 = (void *)piVar10[-1];
                  piVar10 = piVar10 + -1;
                  if (pvVar2 != (void *)0x0) {
                    FUN_0040b740();
                    operator_delete(pvVar2);
                  }
                }
                if (*local_30 == 0) {
                  free(local_30);
                }
              }
              FUN_0040ba80();
            }
            piVar10 = (int *)*piVar9;
            iVar16 = *(int *)(piVar10[(int)local_50 + piVar10[2] + 5] + 0x5c);
            if (*piVar10 != 1) {
              local_30 = piVar10 + piVar10[2] + 5;
              piVar10 = (int *)FUN_00635250();
              if (piVar10 != (int *)0x0) {
                piStack_28 = piVar10 + piVar10[2] + 5;
                piVar14 = piVar10 + piVar10[3] + 5;
                if (piStack_28 != piVar14) {
                  do {
                    pvVar2 = (void *)piVar14[-1];
                    piVar14 = piVar14 + -1;
                    if (pvVar2 != (void *)0x0) {
                      FUN_0040b740();
                      operator_delete(pvVar2);
                    }
                  } while (piVar14 != piStack_28);
                }
                if (*piVar10 == 0) {
                  free(piVar10);
                }
              }
              FUN_0040ba80();
            }
            FUN_00630420();
            piVar10 = (int *)*piVar9;
            if (*piVar10 != 1) {
              local_30 = piVar10 + piVar10[2] + 5;
              piVar10 = (int *)FUN_00635250();
              if (piVar10 != (int *)0x0) {
                piVar14 = piVar10 + piVar10[3] + 5;
                piVar11 = piVar10 + piVar10[2] + 5;
                piStack_28 = piVar11;
                if (piVar11 != piVar14) {
                  do {
                    pvVar2 = (void *)piVar14[-1];
                    piVar14 = piVar14 + -1;
                    if (pvVar2 != (void *)0x0) {
                      FUN_0040b740();
                      operator_delete(pvVar2);
                      piVar11 = piStack_28;
                    }
                  } while (piVar14 != piVar11);
                }
                if (*piVar10 == 0) {
                  free(piVar10);
                }
              }
              FUN_0040ba80();
            }
            local_58[iVar16 * 2 + 1] =
                 *(int *)(*piVar9 + 0x14 + (*(int *)(*piVar9 + 8) + (int)local_50) * 4);
            piVar10 = (int *)*piVar9;
            iVar16 = piVar10[2];
            local_50 = (int *)((int)local_50 + 1);
          } while ((int)local_50 < piVar10[3] - iVar16);
        }
        if (((**(int **)(param_1 + 8) != 1) &&
            (piVar10 = (int *)FUN_00635250(), piVar10 != (int *)0x0)) && (*piVar10 == 0)) {
          free(piVar10);
        }
        puVar12 = (undefined4 *)FUN_00634c70();
        *puVar12 = piVar9;
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
      }
      if (iVar16 + -1 == 0) {
        QString_freeData();
      }
      local_44 = (int *)((int)local_44 + 1);
    } while ((int)local_44 < local_6c[3] - local_6c[2]);
  }
  bVar3 = false;
  bVar4 = false;
  local_30 = (int *)0x0;
  local_48 = (int *)0x0;
  if (local_6c[3] != local_6c[2] && -1 < local_6c[3] - local_6c[2]) {
LAB_0040f447:
    piVar17 = (int *)local_6c[(int)local_48 + local_6c[2] + 5];
    *piVar17 = *piVar17 + 1;
    QString_fromAscii_maybe();
    iVar16 = FUN_0062fb50();
    if ((iVar16 == -1) && (4 < piVar17[2])) {
      QString_fromAscii_maybe();
      bVar3 = true;
      local_4 = 0x21;
      iVar16 = FUN_0062fb50();
      if (iVar16 != -1) goto LAB_0040f51c;
      QString_fromAscii_maybe();
      bVar3 = true;
      bVar4 = true;
      local_4 = 0x22;
      iVar16 = FUN_0062fb50();
      uStack_7c = (int *)CONCAT13(1,(undefined3)uStack_7c);
      if (iVar16 == -1) goto LAB_0040f51c;
    }
    else {
LAB_0040f51c:
      uStack_7c = (int *)((uint)uStack_7c & 0xffffff);
    }
    local_4 = 0x21;
    if (bVar4) {
      bVar4 = false;
      if (iVar16 + -1 == 0) {
        QString_freeData();
      }
    }
    local_4 = 0x20;
    if (bVar3) {
      bVar3 = false;
      if (iVar16 + -1 == 0) {
        QString_freeData();
      }
    }
    if (iVar16 + -1 == 0) {
      QString_freeData();
    }
    if (uStack_7c._3_1_ == '\0') goto LAB_0040fbf0;
    QString_fromAscii_maybe();
    FUN_00636320();
    FUN_00635bc0();
    FUN_00630470();
    if (iVar16 + -1 == 0) {
      QString_freeData();
    }
    FUN_006361b0();
    FUN_004b7c50();
    local_34 = (int *)(local_6c[3] - local_6c[2]);
    FUN_006a3cb0();
    FUN_004b7ff0();
    local_20 = (int *)&stack0xffffff70;
    FUN_00643bd0();
    cVar6 = FUN_004b74f0();
    if (cVar6 == '\0') {
      local_20 = (int *)&stack0xffffff70;
      FUN_00643bd0();
      QString_fromAscii_maybe();
      FUN_00630ad0();
      if (iVar16 + -1 == 0) {
        QString_freeData();
      }
      local_20 = (int *)FUN_006a3bdf();
      if (local_20 == (int *)0x0) {
        piVar9 = (int *)0x0;
      }
      else {
        *piVar17 = *piVar17 + 1;
        local_24 = (int *)&stack0xffffff64;
        **(int **)(param_1 + 0x1c) = **(int **)(param_1 + 0x1c) + 1;
        piVar9 = (int *)FUN_00423680();
      }
      piVar9[4] = (int)local_30;
      IPL_loadTextFile();
      local_30 = (int *)(piVar9[5] + 10000);
      iVar16 = *(int *)(*piVar9 + 0xc);
      iVar8 = *(int *)(*piVar9 + 8);
      local_50 = (int *)0x0;
      if (iVar16 != iVar8 && -1 < iVar16 - iVar8) {
        do {
          piVar10 = (int *)*piVar9;
          if (*piVar10 != 1) {
            local_20 = piVar10 + piVar10[2] + 5;
            local_24 = (int *)FUN_00635250();
            if (local_24 != (int *)0x0) {
              piVar10 = local_24 + local_24[3] + 5;
              local_34 = local_24 + local_24[2] + 5;
              if (local_34 != piVar10) {
                do {
                  pvVar2 = (void *)piVar10[-1];
                  piVar10 = piVar10 + -1;
                  if (pvVar2 != (void *)0x0) {
                    iVar16 = **(int **)((int)pvVar2 + 8) + -1;
                    **(int **)((int)pvVar2 + 8) = iVar16;
                    if (iVar16 == 0) {
                      QString_freeData();
                    }
                    operator_delete(pvVar2);
                  }
                } while (piVar10 != local_34);
              }
              if (*local_24 == 0) {
                free(local_24);
              }
            }
            FUN_0040b6c0();
          }
          piVar10 = (int *)*piVar9;
          local_34 = *(int **)(piVar10[(int)local_50 + piVar10[2] + 5] + 4);
          if (*piVar10 != 1) {
            local_24 = piVar10 + piVar10[2] + 5;
            piVar10 = (int *)FUN_00635250();
            if (piVar10 != (int *)0x0) {
              local_20 = piVar10 + piVar10[2] + 5;
              piVar14 = piVar10 + piVar10[3] + 5;
              if (local_20 != piVar14) {
                do {
                  pvVar2 = (void *)piVar14[-1];
                  piVar14 = piVar14 + -1;
                  if (pvVar2 != (void *)0x0) {
                    iVar16 = **(int **)((int)pvVar2 + 8) + -1;
                    **(int **)((int)pvVar2 + 8) = iVar16;
                    if (iVar16 == 0) {
                      QString_freeData();
                    }
                    operator_delete(pvVar2);
                  }
                } while (piVar14 != local_20);
              }
              if (*piVar10 == 0) {
                free(piVar10);
              }
            }
            FUN_0040b6c0();
          }
          *(int *)(*(int *)(*piVar9 + 0x14 + (*(int *)(*piVar9 + 8) + (int)local_50) * 4) + 0x68) =
               local_58[(int)local_34 * 2 + 1];
          piVar10 = (int *)*piVar9;
          if (*piVar10 != 1) {
            local_24 = piVar10 + piVar10[2] + 5;
            piVar10 = (int *)FUN_00635250();
            if (piVar10 != (int *)0x0) {
              piVar14 = piVar10 + piVar10[3] + 5;
              local_20 = piVar10 + piVar10[2] + 5;
              if (local_20 != piVar14) {
                do {
                  pvVar2 = (void *)piVar14[-1];
                  piVar14 = piVar14 + -1;
                  if (pvVar2 != (void *)0x0) {
                    iVar16 = **(int **)((int)pvVar2 + 8) + -1;
                    **(int **)((int)pvVar2 + 8) = iVar16;
                    if (iVar16 == 0) {
                      QString_freeData();
                    }
                    operator_delete(pvVar2);
                  }
                } while (piVar14 != local_20);
              }
              if (*piVar10 == 0) {
                free(piVar10);
              }
            }
            FUN_0040b6c0();
          }
          piVar10 = (int *)*piVar9;
          if (*(int *)(piVar10[(int)local_50 + piVar10[2] + 5] + 0x68) != 0) {
            if (*piVar10 != 1) {
              local_24 = piVar10 + piVar10[2] + 5;
              piVar10 = (int *)FUN_00635250();
              if (piVar10 != (int *)0x0) {
                local_20 = piVar10 + piVar10[2] + 5;
                piVar14 = piVar10 + piVar10[3] + 5;
                if (local_20 != piVar14) {
                  do {
                    pvVar2 = (void *)piVar14[-1];
                    piVar14 = piVar14 + -1;
                    if (pvVar2 != (void *)0x0) {
                      iVar16 = **(int **)((int)pvVar2 + 8) + -1;
                      **(int **)((int)pvVar2 + 8) = iVar16;
                      if (iVar16 == 0) {
                        QString_freeData();
                      }
                      operator_delete(pvVar2);
                    }
                  } while (piVar14 != local_20);
                }
                if (*piVar10 == 0) {
                  free(piVar10);
                }
              }
              FUN_0040b6c0();
            }
            if (*(int *)*piVar9 != 1) {
              FUN_0040bad0();
            }
            FUN_00630420();
          }
          local_50 = (int *)((int)local_50 + 1);
        } while ((int)local_50 < *(int *)(*piVar9 + 0xc) - *(int *)(*piVar9 + 8));
      }
      if (((**(int **)(param_1 + 4) != 1) &&
          (piVar10 = (int *)FUN_00635250(), piVar10 != (int *)0x0)) && (*piVar10 == 0)) {
        free(piVar10);
      }
      puVar12 = (undefined4 *)FUN_00634c70();
      *puVar12 = piVar9;
      if (iVar16 + -1 == 0) {
        QString_freeData();
      }
      goto LAB_0040fbf0;
    }
    (**(code **)(*local_60 + 0xd0))();
    if (iVar16 + -1 == 0) {
      QString_freeData();
    }
    if (iVar16 + -1 == 0) {
      QString_freeData();
    }
    if (local_6c != (int *)0x0) {
      if (iVar16 + -1 == 0) {
        iVar16 = local_6c[2];
        piVar17 = local_6c + local_6c[3] + 5;
        while (local_6c + iVar16 + 5 != piVar17) {
          piVar9 = piVar17 + -1;
          piVar17 = piVar17 + -1;
          iVar8 = *(int *)*piVar9 + -1;
          *(int *)*piVar9 = iVar8;
          if (iVar8 == 0) {
            QString_freeData();
          }
        }
        if (*local_6c == 0) {
          free(local_6c);
        }
      }
    }
    FUN_00642f50();
    FUN_00641a10();
    FUN_006361b0();
    if (iVar16 + -1 != 0) {
      return 0;
    }
    goto LAB_00410bca;
  }
LAB_0040fc2f:
  bVar3 = false;
  bVar4 = false;
  if (local_58 != (int *)0x0) {
    iVar16 = *(int *)*local_58 + -1;
    *(int *)*local_58 = iVar16;
    if (iVar16 == 0) {
      QString_freeData();
    }
    operator_delete(local_58);
  }
  local_20 = (int *)FUN_006a3bdf();
  if (local_20 == (int *)0x0) {
    uVar7 = 0;
  }
  else {
    uVar7 = FUN_00405ce0();
  }
  *(undefined4 *)(param_1 + 0x24) = uVar7;
  DAT_007af5a0 = DAT_007af5a0 + 1;
  uVar5 = (undefined1)local_4;
  if ((*(byte *)(param_1 + 0x10) < 3) &&
     (iVar16 = 0, uVar5 = (undefined1)local_4,
     local_6c[3] != local_6c[2] && -1 < local_6c[3] - local_6c[2])) {
    do {
      piVar17 = (int *)local_6c[local_6c[2] + iVar16 + 5];
      *piVar17 = *piVar17 + 1;
      QString_fromAscii_maybe();
      iVar8 = FUN_0062fb50();
      if ((iVar8 == -1) && (4 < piVar17[2])) {
        QString_fromAscii_maybe();
        bVar3 = true;
        local_4 = 0x2c;
        iVar8 = FUN_0062fb50();
        if (iVar8 != -1) goto LAB_0040feb5;
        QString_fromAscii_maybe();
        bVar3 = true;
        bVar4 = true;
        local_4 = 0x2d;
        iVar8 = FUN_0062fb50();
        uStack_7c = (int *)CONCAT13(1,(undefined3)uStack_7c);
        if (iVar8 == -1) goto LAB_0040feb5;
      }
      else {
LAB_0040feb5:
        uStack_7c = (int *)((uint)uStack_7c & 0xffffff);
      }
      local_4 = 0x2c;
      if (bVar4) {
        bVar4 = false;
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
      }
      local_4 = 0x2b;
      if (bVar3) {
        bVar3 = false;
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
      }
      if (uStack_7c._3_1_ != '\0') {
        QString_fromAscii_maybe();
        FUN_00630ad0();
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
        QString_fromAscii_maybe();
        FUN_00630ad0();
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
        QString_fromAscii_maybe();
        FUN_00630ad0();
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
        QString_fromAscii_maybe();
        FUN_00630ad0();
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
        QString_fromAscii_maybe();
        FUN_00630ad0();
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
        local_58 = *(int **)(param_1 + 0x1c);
        *local_58 = *local_58 + 1;
        QString_assignLiteral_maybe();
        FUN_00630470();
        QString_fromAscii_maybe();
        QString_fromAscii_maybe();
        FUN_00630d70();
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
        FUN_0040c490();
        if (iVar8 + -1 == 0) {
          QString_freeData();
        }
      }
      if (iVar8 + -1 == 0) {
        QString_freeData();
      }
      iVar16 = iVar16 + 1;
      uVar5 = (undefined1)local_4;
    } while (iVar16 < local_6c[3] - local_6c[2]);
  }
  if (*(char *)(param_1 + 0x10) == '\x02') {
    piVar17 = (int *)(param_1 + 8);
    FUN_004b7ff0();
    iVar16 = *(int *)(*piVar17 + 0xc);
    iVar8 = *(int *)(*piVar17 + 8);
    iVar15 = 0;
    if (iVar16 != iVar8 && -1 < iVar16 - iVar8) {
      do {
        if (((*(int *)*piVar17 != 1) && (piVar9 = (int *)FUN_00635250(), piVar9 != (int *)0x0)) &&
           (*piVar9 == 0)) {
          free(piVar9);
        }
        piVar9 = *(int **)(*(int *)(*piVar17 + 0x14 + (*(int *)(*piVar17 + 8) + iVar15) * 4) + 0xc);
        *piVar9 = *piVar9 + 1;
        FUN_00633500();
        FUN_00630420();
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
        QString_fromAscii_maybe();
        QString_fromAscii_maybe();
        FUN_00630d70();
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
        QString_fromAscii_maybe();
        FUN_00636320();
        FUN_00635bc0();
        FUN_00630470();
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
        FUN_006361b0();
        local_58 = *(int **)(param_1 + 0x1c);
        *local_58 = *local_58 + 1;
        QString_assignLiteral_maybe();
        FUN_00630470();
        QString_fromAscii_maybe();
        QString_fromAscii_maybe();
        FUN_00630d70();
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
        FUN_0040c490();
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
        iVar15 = iVar15 + 1;
      } while (iVar15 < *(int *)(*piVar17 + 0xc) - *(int *)(*piVar17 + 8));
    }
  }
  if ((*(char *)(param_1 + 0x10) == '\x03') &&
     (local_30 = (int *)0x0, 0 < *(int *)(**(int **)(param_1 + 0xc) + 8))) {
    iVar16 = 0x10;
    do {
      if (*(int *)**(undefined4 **)(param_1 + 0xc) != 1) {
        FUN_0058fc70();
      }
      FUN_004b7ff0();
      iVar8 = 0;
      while( true ) {
        piVar17 = *(int **)(param_1 + 0xc);
        if (*(int *)*piVar17 != 1) {
          FUN_0058fc70();
        }
        iVar15 = *(int *)(*(int *)(iVar16 + *piVar17) + 0xc);
        if (*(int *)(iVar15 + 0xc) - *(int *)(iVar15 + 8) <= iVar8) break;
        QString_fromAscii_maybe();
        piVar17 = *(int **)(param_1 + 0xc);
        if (*(int *)*piVar17 != 1) {
          FUN_0058fc70();
        }
        if (**(int **)(*(int *)(iVar16 + *piVar17) + 0xc) != 1) {
          FUN_0040c710();
        }
        iVar13 = FUN_0062fb50();
        if (iVar15 + -1 == 0) {
          QString_freeData();
        }
        if (iVar13 != -1) {
          piVar17 = *(int **)(param_1 + 0xc);
          if (*(int *)*piVar17 != 1) {
            FUN_0058fc70();
          }
          if (**(int **)(*(int *)(*piVar17 + iVar16) + 0xc) != 1) {
            FUN_0040c710();
          }
          FUN_0040c490();
        }
        iVar8 = iVar8 + 1;
      }
      local_30 = (int *)((int)local_30 + 1);
      iVar16 = iVar16 + 4;
    } while ((int)local_30 < *(int *)(**(int **)(param_1 + 0xc) + 8));
  }
  local_30 = (int *)0x0;
  if (DAT_007af5ac != DAT_007af5a8 && -1 < DAT_007af5ac - DAT_007af5a8) {
    do {
      piVar17 = local_30;
      QString_fromAscii_maybe();
      if (DAT_007af5a0 != 1) {
        FUN_0040b1e0();
      }
      FUN_00636320();
      FUN_00635bc0();
      FUN_00630470();
      if (iVar16 + -1 == 0) {
        QString_freeData();
      }
      FUN_006361b0();
      FUN_004b7c50();
      local_24 = (int *)(DAT_007af5ac - DAT_007af5a8);
      FUN_006a3cb0();
      FUN_004b7ff0();
      local_24 = (int *)&stack0xffffff70;
      FUN_00643bd0();
      cVar6 = FUN_004b74f0();
      if (cVar6 != '\0') {
        (**(code **)(*local_60 + 0xd0))();
        if (iVar16 + -1 == 0) {
          QString_freeData();
        }
        DAT_007af5a0 = DAT_007af5a0 + -1;
        if ((DAT_007af5a0 == 0) && (FUN_0040a110(), DAT_007af5a0 == 0)) {
          free(&DAT_007af5a0);
        }
        if (local_6c != (int *)0x0) {
          if ((iVar16 + -1 == 0) && (FUN_0040a110(), *local_6c == 0)) {
            free(local_6c);
          }
        }
        FUN_00642f50();
        goto LAB_00410b8b;
      }
      if (*(char *)(param_1 + 0x10) == '\x03') {
        if (DAT_007af5a0 != 1) {
          FUN_0040b1e0();
        }
        local_24 = (int *)&stack0xffffff70;
        **(int **)(&DAT_007af5b4 + (DAT_007af5a8 + (int)piVar17) * 4) =
             **(int **)(&DAT_007af5b4 + (DAT_007af5a8 + (int)piVar17) * 4) + 1;
        FUN_00407bc0();
      }
      else {
        if (DAT_007af5a0 != 1) {
          FUN_0040b1e0();
        }
        local_24 = (int *)&stack0xffffff70;
        **(int **)(&DAT_007af5b4 + (DAT_007af5a8 + (int)piVar17) * 4) =
             **(int **)(&DAT_007af5b4 + (DAT_007af5a8 + (int)piVar17) * 4) + 1;
        FUN_00407a30();
      }
      if (iVar16 + -1 == 0) {
        QString_freeData();
      }
      local_30 = (int *)((int)piVar17 + 1);
    } while ((int)local_30 < DAT_007af5ac - DAT_007af5a8);
  }
  DAT_007af5a0 = DAT_007af5a0 + -1;
  if ((DAT_007af5a0 == 0) && (FUN_0040a110(), DAT_007af5a0 == 0)) {
    free(&DAT_007af5a0);
  }
  if (local_6c != (int *)0x0) {
    if ((iVar16 + -1 == 0) && (FUN_0040a110(), *local_6c == 0)) {
      free(local_6c);
    }
  }
  FUN_00642f50();
  *(undefined1 *)(param_1 + 0x2c) = 1;
  std::operator<<<std::char_traits<char>_>
            ((basic_ostream<char,struct_std::char_traits<char>_> *)cout_exref,"Task Completed\n");
  (**(code **)(*local_60 + 0x2c))();
  (**(code **)(*local_60 + 0xc))();
  pvStack_c._0_1_ = 3;
  FUN_00641a10();
  pvStack_c = (void *)CONCAT31(pvStack_c._1_3_,1);
  FUN_006361b0();
  pvStack_c = (void *)0xffffffff;
  if (iVar16 + -1 == 0) {
    QString_freeData();
  }
  return 1;
LAB_0040fbf0:
  if (iVar16 + -1 == 0) {
    QString_freeData();
  }
  local_48 = (int *)((int)local_48 + 1);
  if (local_6c[3] - local_6c[2] <= (int)local_48) goto LAB_0040fc2f;
  goto LAB_0040f447;
}
