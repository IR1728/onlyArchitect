# Análisis de Ingeniería Inversa: easetup_x64.msi

Documento técnico con los resultados del análisis estático de [easetup_x64.msi](file:///home/iran/EA/easetup_x64.msi), estructura del instalador MSI/OLE, tablas de secuencia, binarios de Custom Actions embebidos, pseudocódigo decompilado, cadenas, símbolos, llamadas y registro de evidencia.

---

## 1. Identidad del Artefacto y Arquitectura del Contenedor MSI

* **Archivo**: [easetup_x64.msi](file:///home/iran/EA/easetup_x64.msi) (`346,419,200` bytes)
* **SHA-256**: `8b7311735840bd3196a84681803e20419a30b25264bcd67c6d21224ae8e3dca8`
* **Formato**: OLE2 Composite Document File (Microsoft Windows Installer, Codepage `1252`, `x64;1033`)
* **Metadatos (`Property` / `\x05SummaryInformation`)**:
  * **ProductName**: `Enterprise Architect Trial`
  * **ProductVersion**: `17.2.1721.15`
  * **Manufacturer**: `Sparx Systems`
  * **ProductCode**: `{17C51F4B-53A6-4FD9-9E29-73F6394E9C1C}`
  * **UpgradeCode**: `{878687A9-43A8-4374-A3C7-2F71C5141B90}`
  * **Generador**: `Windows Installer XML Toolset` (WiX `3.8.1128.0`)
* **Firma Authenticode (`\x05DigitalSignature`, 10,408 bytes)**:
  * **Firmante**: `CN=Sparx Systems Pty Ltd, O=Sparx Systems Pty Ltd, L=Creswick, ST=Victoria, C=AU`
  * **Emisor**: `CN=DigiCert Trusted G4 Code Signing RSA4096 SHA384 2021 CA1`

### Streams Embebidos en el OLE CFB
El archivo almacena 48 tablas MSI (`17,501` cadenas en el pool), 4 archivos Cabinet (`.cab`) con 3,744 archivos (`1,633,688,384` bytes sin comprimir) y 2 DLLs PE x86 en la tabla `Binary`:

| Stream OLE | Tamaño (bytes) | Tipo / Propósito | Identidad / SHA-256 |
| :--- | ---: | :--- | :--- |
| `Binary.WixCA` | `172,032` | PE32 DLL (`wixca.dll` v3.8.1128.0) | `b9e73e5ab78d033e0328fc74a9e4ebbd1af614bc4a7c894beb8c59d24ee3ede9` |
| `Binary.WixUIWixca` | `76,288` | PE32 DLL (`uica.dll` v3.8.1128.0) | `0fd225cb064e60e864a001c687274abb3dc774f1820f2afefc6b14b838e939f2` |
| `simple.cab` | `40,126,171` | MS Cabinet (DiskId 1, Seq 1–25) | Incluye `EA.exe` (`152,550,496` B, v17.2.0.1721) |
| `common.cab` | `15,729,785` | MS Cabinet (DiskId 2, Seq 26–649) | Recursos comunes y `License.txt` |
| `nonlite.cab` | `242,653,574` | MS Cabinet (DiskId 3, Seq 650–3665) | Plantillas, MDG, agentes Linux (`agent`, `gdbagent`, `satellite`) |
| `commonx64.cab` | `45,498,107` | MS Cabinet (DiskId 4, Seq 3666–3744) | Binarios de 64 bits (`Cairo172-64.dll`, `SSSimRunner64.dll`, etc.) |

---

## 2. Lógica Central del Instalador (`InstallUISequence` e `InstallExecuteSequence`)

El motor de Windows Installer evalúa secuencialmente las acciones e invoca funciones de soporte nativo:

```
[msiexec.exe Execution Flow]
   │
   ├─► Seq 1:    SystemFolder.246EB7AD... (Type 51: asigna propiedad de directorio SystemFolder)
   ├─► Seq 25:   FindRelatedProducts      (Busca UpgradeCode {878687A9-...} -> WIX_UPGRADE_DETECTED)
   ├─► Seq 50:   AppSearch                (Busca HKCU\Software\Wine\Debug\RelayExclude -> WINECHECK)
   ├─► Seq 800+: CostInitialize -> FileCost -> CostFinalize -> MigrateFeatureStates
   │     ├─ Condición Feature MSXML:         Level=0 si VersionNT >= 600 (se desactiva en Vista+)
   │     └─ Condición Feature SSFileHandler: Level=0 si VersionNT < 600
   │
   ├─► [Fase UI: WelcomeDlg -> LicenseAgreementDlg -> ReadmeDlg -> InstallDirDlg -> VerifyReadyDlg]
   │     ├─ Botón "Print" en LicenseAgreementDlg ──► DoAction: WixUIPrintEula   (uica.dll!PrintEula)
   │     └─ Botón "Next"  en InstallDirDlg       ──► DoAction: WixUIValidatePath (uica.dll!ValidatePath)
   │
   └─► [Fase Execute: InstallInitialize (1500) ... InstallFinalize (6600)]
         ├─ Seq 1401: RemoveExistingProducts
         ├─ Seq 3599: WixSchedInternetShortcuts    (wixca.dll!WixSchedInternetShortcuts, inmediata)
         ├─ Seq 3800: MoveFiles                    (Mueve icui30.dll de FIREBIRDDIR a INSTALLDIR)
         ├─ Seq 4000: InstallFiles                 (Extrae 3,744 archivos a ProgramFiles64Folder\Sparx Systems\EA Trial)
         ├─ Seq 4500: CreateShortcuts              (Crea accesos directos .lnk en Menú Inicio y Escritorio)
         ├─ Seq 4501: WixRollbackInternetShortcuts (wixca.dll!WixRollbackInternetShortcuts, diferida rollback)
         ├─ Seq 4502: WixCreateInternetShortcuts   (wixca.dll!WixCreateInternetShortcuts, diferida system)
         ├─ Seq 5000: WriteRegistryValues          (Escribe 530 claves de registro: COM CLSIDs, TypeLibs, ProgIDs)
         ├─ Seq 6596: linuxagentExecutable         (Condición: $linuxagent=3 and WINECHECK<>"")
         ├─ Seq 6597: linuxgdbagentExecutable      (Condición: $linuxgdbagent=3 and WINECHECK<>"")
         ├─ Seq 6598: linuxsatelliteExecutable     (Condición: $linuxsatellite=3 and WINECHECK<>"")
         ├─ Seq 6599: RegisterEA                   (Condición: $EA.exe=3 -> ejecuta EA.exe /register)
         └─ Seq 6600: InstallFinalize
```

### Tabla `CustomAction`

| Action | Type (Dec / Flags) | Source | Target / Punto de Entrada | Condición / Disparador |
| :--- | :--- | :--- | :--- | :--- |
| `SystemFolder.246EB7AD...` | `51` (Property Set) | `SystemFolder.246...` | `[SystemFolder]` | Seq `1` (UI & Execute) |
| `WixUIValidatePath` | `65` (DLL C.A., Ignore Return) | `WixUIWixca` (`uica.dll`) | `ValidatePath` (`0x100010fe`) | UI `InstallDirDlg` -> `Next` (`NOT WIXUI_DONTVALIDATEPATH`) |
| `WixUIPrintEula` | `65` (DLL C.A., Ignore Return) | `WixUIWixca` (`uica.dll`) | `PrintEula` (`0x10001a66`) | UI `LicenseAgreementDlg` -> `Print` |
| `WixSchedInternetShortcuts` | `1` (DLL C.A., Immediate) | `WixCA` (`wixca.dll`) | `WixSchedInternetShortcuts` (`0x10001eae`) | Seq `3599` (`VersionNT > 400`) |
| `WixRollbackInternetShortcuts` | `3329` (`0xD01`: Rollback) | `WixCA` (`wixca.dll`) | `WixRollbackInternetShortcuts` (`0x10001d9a`) | Seq `4501` (`VersionNT > 400`) |
| `WixCreateInternetShortcuts` | `3073` (`0xC01`: Deferred) | `WixCA` (`wixca.dll`) | `WixCreateInternetShortcuts` (`0x10001c1e`) | Seq `4502` (`VersionNT > 400`) |
| `linuxagentExecutable` | `3186` (`0xC72`: Deferred) | `BuiltInStart` (`start.exe`)| `/bin/sh chmod u+x "\`winepath -u "[#linuxagent]"\`"` | Seq `6596` (`$linuxagent=3 and WINECHECK<>""`) |
| `linuxgdbagentExecutable` | `3186` (`0xC72`: Deferred) | `BuiltInStart` | `/bin/sh chmod u+x "\`winepath -u "[#linuxgdbagent]"\`"`| Seq `6597` (`$linuxgdbagent=3 and WINECHECK<>""`) |
| `linuxsatelliteExecutable` | `3186` (`0xC72`: Deferred) | `BuiltInStart` | `/bin/sh chmod u+x "\`winepath -u "[#linuxsatellite]"\`"`| Seq `6598` (`$linuxsatellite=3 and WINECHECK<>""`) |
| `RegisterEA` | `3154` (`0xC52`: Deferred) | `EA.exe` | `/register` | Seq `6599` (`$EA.exe=3`) |

---

## 3. Símbolos, Importaciones y Cadenas Clave

### 3.1 Símbolos Exportados
* **`Binary.WixCA` (`wixca.dll`, ImageBase `0x10000000`, 696 procedimientos, 1,105 cadenas)**:
  * Exporta 33 funciones de WiX. Tres son referenciadas directamente por la tabla `CustomAction`:
    * `WixSchedInternetShortcuts` (`0x10001eae`)
    * `WixCreateInternetShortcuts` (`0x10001c1e`)
    * `WixRollbackInternetShortcuts` (`0x10001d9a`)
* **`Binary.WixUIWixca` (`uica.dll`, ImageBase `0x10000000`, 418 procedimientos, 377 cadenas)**:
  * Exporta 2 funciones (ambas referenciadas por la UI):
    * `ValidatePath` (`0x100010fe`)
    * `PrintEula` (`0x10001a66`)

### 3.2 Resolución de Importaciones por Ordinal de `msi.dll` (`wcautil`)

| Ordinal `msi.dll` | Función API en `msi.dll` | Wrapper en `wixca.dll` | Wrapper en `uica.dll` | Rol en WiX (`wcautil`) |
| :--- | :--- | :--- | :--- | :--- |
| `Ordinal_8` | `MsiCloseHandle` | `0x1000c999` | `0x100024bf` | `WcaFinalize` |
| `Ordinal_17` | `MsiCreateRecord` | `0x1000dc6d`, `0x1000e311` | — | `WcaGetRecordFormattedString`, `WcaProgressMessage` |
| `Ordinal_26` | `MsiDatabaseGetPrimaryKeysW` | `0x1000e642` | — | `WcaTableExists` |
| `Ordinal_32` | `MsiDatabaseOpenViewW` | `0x1000e1b8` | `0x10002dfa` | `WcaOpenExecuteView` |
| `Ordinal_49` | `MsiGetActiveDatabase` | `0x1000ca09` | `0x1000252f` | `WcaInitialize` |
| `Ordinal_51` | `MsiGetComponentStateW` | `0x1000d836` | — | `WcaGetComponentToDo` |
| `Ordinal_74` | `MsiGetPropertyW` | `0x1000db43` | `0x10002b91` | `WcaGetProperty` |
| `Ordinal_80` | `MsiGetTargetPathW` | `0x1000dfe3` | — | `WcaGetTargetPath` |
| `Ordinal_116` | `MsiRecordGetInteger` | `0x1000ddc2` | — | `WcaGetRecordInteger` |
| `Ordinal_118` | `MsiRecordGetStringW` | `0x1000ded4` | `0x10002cbb` | `WcaGetRecordString` |
| `Ordinal_121` | `MsiRecordSetInteger` | `0x1000e311`, `0x1000d0a1` | — | `WcaProgressMessage`, `WcaAddTempRecord` |
| `Ordinal_125` | `MsiRecordSetStringW` | `0x1000d0a1` | — | `WcaAddTempRecord` |
| `Ordinal_145` | `MsiSetPropertyW` | `0x1000e56a` | `0x10002ee2` | `WcaSetProperty` |
| `Ordinal_159` | `MsiViewExecute` | `0x1000e1b8` | `0x10002dfa` | `WcaOpenExecuteView` |
| `Ordinal_160` | `MsiViewFetch` | `0x1000d792` | `0x10002b3d` | `WcaFetchRecord` |
| `Ordinal_163` | `MsiViewModify` | `0x1000d0a1` | — | `WcaAddTempRecord` |
| `Ordinal_171` | `MsiFormatRecordW` | `0x1000dc6d` | — | `WcaGetRecordFormattedString` |

### 3.3 GUIDs COM y Cadenas Relevantes
* **GUIDs COM (`wixca.dll` .rdata)**:
  * `0x10022b10`: `CLSID_InternetShortcut` (`{FBF23B40-E3F0-101B-8488-00AA003E56F8}`)
  * `0x10022b20`: `IID_IUniformResourceLocatorW` (`{CABB0DA0-DA57-11CF-9974-0020AFD79762}`)
  * `0x10022b40`: `CLSID_ShellLink` (`{00021401-0000-0000-C000-000000000046}`)
  * `0x10022b00`: `IID_IShellLinkW` (`{000214F9-0000-0000-C000-000000000046}`)
  * `0x10022b30`: `IID_IPersistFile` (`{0000010B-0000-0000-C000-000000000046}`)
* **Cadenas clave**:
  * `"SELECT \`Component_\`, \`Directory_\`, \`Name\`, \`Target\`, \`Attributes\` FROM \`WixInternetShortcut\`"`
  * `"WixInternetShortcut table doesn't exist, so there are no Internet shortcuts to process"`
  * `"Adding folder '%ls', component '%ls' to the CreateFolder table"`
  * `"Creating IShellLinkW shortcut '%ls' target '%ls'"`
  * `"Creating IUniformResourceLocatorW shortcut '%ls' target '%ls'"`
  * `"SELECT \`Text\` FROM \`Control\` WHERE \`Control\`='LicenseText'"`
  * `"Installation path %ls is invalid: it is %s UNC path, %s remote path, or %s path on a removable drive, and must be none of these."`
  * `"Riched20.dll"`

---

## 4. Pseudocódigo Decompilado de las Funciones Principales

### 4.1 `WixSchedInternetShortcuts` (`wixca.dll` @ `0x10001eae`)

```c
/* wixca.dll @ 0x10001eae — Export #30: WixSchedInternetShortcuts */
int WixSchedInternetShortcuts(int /*MSIHANDLE*/ hInstall)
{
  bool bComInitialized = false;
  uint hr;
  HRESULT hRes;
  int cShortcuts = 0;
  int *piUrl = NULL;         /* IUniformResourceLocatorW* */
  int *piShellLink = NULL;   /* IShellLinkW* */
  int iAttributes = 0;
  uint hColumns = 0, hTableView = 0;
  LPCWSTR wzTarget = NULL, wzPath = NULL;
  short *wzFilename = NULL, *wzDirectory = NULL, *wzCustomActionData = NULL;
  LPVOID wzComponent = NULL;
  int hView = 0;
  uint hRec = 0;
  int todo;

  hr = WcaInitialize(hInstall, "WixSchedInternetShortcuts");
  if ((int)hr < 0) {
    WcaLogError(hr, "failed to initialize WixSchedInternetShortcuts.");
    goto LExit;
  }

  if (WcaTableExists(L"WixInternetShortcut") != 0) {
    WcaLog(2, "WixInternetShortcut table doesn't exist, so there are no Internet shortcuts to process");
    goto LCleanup;
  }

  hr = CoInitialize(NULL);
  if ((int)hr < 0) {
    WcaLogError(hr, "failed to initialize COM");
    goto LCleanup;
  }
  bComInitialized = true;

  hRes = CoCreateInstance(&CLSID_InternetShortcut, NULL, CLSCTX_ALL, &IID_IUniformResourceLocatorW, &piUrl);
  if (hRes == S_OK) {
    hRes = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_ALL, &IID_IShellLinkW, &piShellLink);
    if (hRes == S_OK) {
      hr = WcaOpenExecuteView(
          L"SELECT `Component_`, `Directory_`, `Name`, `Target`, `Attributes` FROM `WixInternetShortcut`",
          &hView);
      if ((int)hr < 0) {
        WcaLogError(hr, "failed to open view on WixInternetShortcut table");
        goto LCleanup;
      }

      while (true) {
        if (hRec != 0) { MsiCloseHandle(hRec); hRec = 0; }
        hr = WcaFetchRecord(hView, &hRec);
        if (hr != 0) {
          if (hr == 0x80070103 /*E_NOMOREITEMS*/) hr = 0;
          if ((int)hr < 0) {
            WcaLogError(hr, "Failure occured while processing WixInternetShortcut table");
            goto LCleanup;
          }
          if (wzCustomActionData == NULL || *wzCustomActionData == 0) goto LCleanup;

          hr = WcaProgressMessage(cShortcuts, 1 /*fExtendProgressBar*/);
          hr = WcaSetProperty(L"WixRollbackInternetShortcuts", wzCustomActionData);
          hr = WcaSetProperty(L"WixCreateInternetShortcuts", wzCustomActionData);
          goto LCleanup;
        }

        WcaGetRecordString(hRec, 1, &wzComponent);       /* "OnlineUserGuide" */
        WcaGetRecordString(hRec, 2, &wzDirectory);       /* "ApplicationProgramsFolder" */
        WcaGetRecordString(hRec, 3, &wzFilename);        /* "Online User Guide.lnk" */
        WcaGetRecordFormattedString(hRec, 4, &wzTarget); /* "https://sparxsystems.com/..." */
        WcaGetRecordInteger(hRec, 5, &iAttributes);      /* 0 */

        todo = WcaGetComponentToDo(wzComponent);
        if (todo == 1 /*WCA_TODO_INSTALL*/ || todo == 2 /*WCA_TODO_REINSTALL*/) {
          int iError = 0;
          hr = WcaAddTempRecord(&hTableView, &hColumns, L"CreateFolder", &iError, 0, 2, wzDirectory, wzComponent);
          WcaGetTargetPath(wzDirectory, &wzPath);
          PathConcat(&wzPath, wzFilename, 0);

          WcaWriteStringToCaData(wzPath, &wzCustomActionData);
          WcaWriteStringToCaData(wzTarget, &wzCustomActionData);
          WcaWriteIntegerToCaData(iAttributes, &wzCustomActionData);
          cShortcuts += 2000;
        }
      }
    }
  }
LCleanup:
  if (bComInitialized) CoUninitialize();
LExit:
  return WcaFinalize(((int)hr >= 0) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE);
}
```

### 4.2 `WixCreateInternetShortcuts` (`wixca.dll` @ `0x10001c1e`) y Helper COM (`0x10001a69`)

```c
/* wixca.dll @ 0x10001c1e — Export #20: WixCreateInternetShortcuts */
void WixCreateInternetShortcuts(undefined4 hInstall)
{
  bool bComInitialized = false;
  uint hr;
  uint uAttributes = 0;
  short *wzCustomActionData = NULL, *pwz = NULL;
  LPVOID wzTarget = NULL, wzPath = NULL;

  hr = WcaInitialize(hInstall, "WixCreateInternetShortcuts");
  if ((int)hr >= 0) {
    hr = CoInitialize(NULL);
    if ((int)hr >= 0) {
      bComInitialized = true;
      hr = WcaGetProperty(L"CustomActionData", (int *)&wzCustomActionData);
      if ((int)hr >= 0 && wzCustomActionData != NULL) {
        pwz = wzCustomActionData;
        while (pwz != NULL && *pwz != 0) {
          WcaReadStringFromCaData(&pwz, (int *)&wzPath);
          WcaReadStringFromCaData(&pwz, (int *)&wzTarget);
          WcaReadIntegerFromCaData(&pwz, (long *)&uAttributes);

          if ((uAttributes & 1) == 0) {
            hr = CreateLinkShortcut(wzTarget, wzPath);
          } else {
            hr = CreateUrlShortcut(wzTarget, wzPath);
          }
          if ((int)hr < 0) {
            WcaLogError(hr, "failed to create Internet shortcut");
            break;
          }
          WcaProgressMessage(2000, 0);
        }
      }
    }
  }
  if (bComInitialized) CoUninitialize();
  WcaFinalize(((int)hr < 0) ? 0x643 : 0);
}

/* wixca.dll @ 0x10001a69 — Helper IShellLinkW + IPersistFile */
HRESULT CreateLinkShortcut(LPCWSTR wzTarget, LPCWSTR wzPath)
{
  IShellLinkW *piShellLink = NULL;
  IPersistFile *piPersistFile = NULL;
  WcaLog(2, "Creating IShellLinkW shortcut '%ls' target '%ls'");
  HRESULT hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_ALL, &IID_IShellLinkW, (void **)&piShellLink);
  if (hr >= 0) {
    hr = piShellLink->lpVtbl->SetPath(piShellLink, wzTarget);
    if (hr >= 0) {
      hr = piShellLink->lpVtbl->QueryInterface(piShellLink, &IID_IPersistFile, (void **)&piPersistFile);
      if (hr >= 0) {
        hr = piPersistFile->lpVtbl->Save(piPersistFile, wzPath, TRUE);
      }
    }
  }
  if (piPersistFile) piPersistFile->lpVtbl->Release(piPersistFile);
  if (piShellLink)   piShellLink->lpVtbl->Release(piShellLink);
  return hr;
}
```

### 4.3 `WixRollbackInternetShortcuts` (`wixca.dll` @ `0x10001d9a`)

```c
/* wixca.dll @ 0x10001d9a — Export #29: WixRollbackInternetShortcuts */
void WixRollbackInternetShortcuts(undefined4 hInstall)
{
  uint hr;
  long lAttributes = 0;
  short *wzCustomActionData = NULL, *pwz = NULL;
  LPCWSTR wzPath = NULL;

  hr = WcaInitialize(hInstall, "WixRemoveInternetShortcuts");
  if ((int)hr >= 0) {
    hr = WcaGetProperty(L"CustomActionData", (int *)&wzCustomActionData);
    if ((int)hr >= 0 && wzCustomActionData != NULL) {
      pwz = wzCustomActionData;
      while (pwz != NULL && *pwz != 0) {
        WcaReadStringFromCaData(&pwz, (int *)&wzPath);
        hr = FileEnsureDelete(wzPath); /* SetFileAttributesW + DeleteFileW */
        if ((int)hr < 0) {
          WcaLogError(hr, "failed to delete file '%ls'");
          break;
        }
        WcaReadStringFromCaData(&pwz, (int *)&wzPath); /* skip target */
        WcaReadIntegerFromCaData(&pwz, &lAttributes);  /* skip attributes */
      }
    }
  }
  WcaFinalize(((int)hr < 0) ? 0x643 : 0);
}
```

### 4.4 `ValidatePath` (`uica.dll` @ `0x100010fe`)

```c
/* uica.dll @ 0x100010fe — Export #1: ValidatePath */
void ValidatePath(undefined4 hInstall)
{
  uint hr;
  BOOL fIsUnc;
  uint fIsRemote = 1;
  int fIsRemovable = 1;
  short *wzInstallDirProperty = NULL;
  LPCWSTR wzInstallDir = NULL;

  hr = WcaInitialize(hInstall, "ValidatePath");
  if ((int)hr >= 0) {
    hr = WcaGetProperty(L"WIXUI_INSTALLDIR", (int *)&wzInstallDirProperty);
    if ((int)hr >= 0) {
      hr = WcaGetProperty(wzInstallDirProperty, (int *)&wzInstallDir);
      if ((int)hr >= 0) {
        IsRemoteDrive(wzInstallDir, &fIsRemote);       /* PathStripToRootW + GetDriveTypeW == 4 */
        IsRemovableDrive(wzInstallDir, &fIsRemovable); /* GetDriveTypeW in {0, 2, 5, 6} */
        fIsUnc = PathIsUNCW(wzInstallDir);

        if (!fIsUnc && fIsRemote == 0 && fIsRemovable == 0) {
          hr = WcaSetProperty(L"WIXUI_INSTALLDIR_VALID", L"1");
        } else {
          WcaLog(2, "Installation path %ls is invalid...");
          hr = WcaSetProperty(L"WIXUI_INSTALLDIR_VALID", L"0");
        }
      }
    }
  }
  WcaFinalize(((int)hr >= 0) ? 0 : 0x643);
}
```

### 4.5 `PrintEula` (`uica.dll` @ `0x10001a66`)

```c
/* uica.dll @ 0x10001a66 — Export #2: PrintEula */
void PrintEula(undefined4 hInstall)
{
  uint hr;
  HWND hWndMain = NULL;
  HMODULE hRichEd = NULL;
  int fRegistered = 0;

  hr = WcaInitialize(hInstall, "PrintEula");
  if ((int)hr >= 0) {
    g_pPrintDlg = (LPPRINTDLGEXW)GlobalAlloc(GPTR, sizeof(PRINTDLGEXW)/*0x54*/);
    g_pPrintDlg->lStructSize = 0x54;
    g_pPrintDlg->hwndOwner   = FindWindowW(L"MsiDialogCloseClass", NULL);
    g_pPrintDlg->Flags       = 0x80011c;
    g_pPrintDlg->nCopies     = 1;
    g_pPrintDlg->nStartPage  = START_PAGE_GENERAL;

    hr = PrintDlgExW(g_pPrintDlg);
    if ((int)hr >= 0 && g_pPrintDlg->dwResultAction == PD_RESULT_PRINT) {
      hr = ReadEulaText(hInstall, (int *)&g_pszEulaText); /* SELECT `Text` FROM `Control` WHERE `Control`='LicenseText' */
      if ((int)hr >= 0) {
        hr = LoadSystemLibrary(L"Riched20.dll", &hRichEd);
        if ((int)hr >= 0) {
          hr = CreateRichTextWindow(&hWndMain, &fRegistered);
          if ((int)hr >= 0) {
            PrintRichText(hWndMain);
          }
        }
      }
    }
  }
  WcaFinalize(0);
}
```

---

## 5. Grafos de Llamadas (Call Graphs)

### 5.1 `wixca.dll`
```
WixSchedInternetShortcuts (0x10001eae)
 ├── WcaInitialize (0x1000ca09) ──► MsiGetActiveDatabase
 ├── WcaTableExists (0x1000e642) ──► MsiDatabaseGetPrimaryKeysW
 ├── ole32.dll!CoInitialize / CoCreateInstance
 ├── WcaOpenExecuteView (0x1000e1b8) ──► MsiDatabaseOpenViewW + MsiViewExecute
 ├── WcaFetchRecord (0x1000d792) ──► MsiViewFetch
 ├── WcaGetRecordString (0x1000ded4) ──► MsiRecordGetStringW
 ├── WcaGetRecordFormattedString (0x1000dc6d) ──► MsiFormatRecordW
 ├── WcaGetRecordInteger (0x1000ddc2) ──► MsiRecordGetInteger
 ├── WcaGetComponentToDo (0x1000d836) ──► MsiGetComponentStateW
 ├── WcaAddTempRecord (0x1000d0a1) ──► MsiViewModify
 ├── WcaGetTargetPath (0x1000dfe3) ──► MsiGetTargetPathW
 ├── WcaProgressMessage (0x1000e311)
 ├── WcaSetProperty (0x1000e56a) ──► MsiSetPropertyW
 └── WcaFinalize (0x1000c999) ──► MsiCloseHandle

WixCreateInternetShortcuts (0x10001c1e)
 ├── WcaInitialize
 ├── CoInitialize
 ├── WcaGetProperty (0x1000db43) ──► MsiGetPropertyW
 ├── CreateLinkShortcut (0x10001a69) ──► IShellLinkW::SetPath + IPersistFile::Save
 ├── CreateUrlShortcut (0x10001b42) ──► IUniformResourceLocatorW::SetURL + IPersistFile::Save
 └── WcaFinalize

WixRollbackInternetShortcuts (0x10001d9a)
 ├── WcaInitialize
 ├── WcaGetProperty
 ├── FileEnsureDelete (0x1000ab60) ──► SetFileAttributesW + DeleteFileW
 └── WcaFinalize
```

### 5.2 `uica.dll`
```
ValidatePath (0x100010fe)
 ├── WcaInitialize
 ├── WcaGetProperty (WIXUI_INSTALLDIR, INSTALLDIR)
 ├── IsRemoteDrive (0x10001024) ──► PathStripToRootW + GetDriveTypeW (== 4)
 ├── IsRemovableDrive (0x10001089) ──► PathStripToRootW + GetDriveTypeW (in {0,2,5,6})
 ├── PathIsUNCW
 ├── WcaSetProperty (WIXUI_INSTALLDIR_VALID, "1" / "0")
 └── WcaFinalize

PrintEula (0x10001a66)
 ├── WcaInitialize
 ├── FindWindowW (L"MsiDialogCloseClass")
 ├── PrintDlgExW
 ├── ReadEulaText (0x1000180a) ──► MsiDatabaseOpenViewW + MsiViewExecute + MsiViewFetch + MsiRecordGetStringW
 ├── LoadSystemLibrary (Riched20.dll)
 ├── CreateRichTextWindow + PrintRichText (GDI32 StartDocW/StartPage/EndPage/EndDoc)
 └── WcaFinalize
```

---

## 6. Registro de Evidencias REA

| ID de Evidencia REA | Operación | Sujeto (`sha256`) | Alcance y Resumen |
| :--- | :--- | :--- | :--- |
| `ev_0783f92cd0367629559ef5972d3f4d899ef1bab4a6d9107046192fb292741041` | `binary_overview` | `wixca.dll` (`b9e73e5a...`) | PE32 x86, Base `0x10000000`, 696 procedimientos, 1,105 cadenas, 6 secciones. |
| `ev_70ce99f5d5ab818275e567e4cab58557b796cb55a4e304b2a68e3f61794b17c1` | `batch_decompile` | `wixca.dll` (`b9e73e5a...`) | Decompilación de `WixSchedInternetShortcuts`, `WixCreateInternetShortcuts` y `WixRollbackInternetShortcuts`. |
| `ev_48fb2103d4bed7deb6d889791fcb36f40cfa5847a81151eb869da82de3e27622` | `batch_decompile` | `wixca.dll` (`b9e73e5a...`) | Decompilación de helpers COM `FUN_10001a69`, `FUN_10001b42`, `FUN_1000d836` y `FUN_1000ca09`. |
| `ev_25a12580ca226ab6c2c56d8a6629f36a2b2d85550567354ba04d94cedd9b3bcd` | `batch_decompile` | `wixca.dll` (`b9e73e5a...`) | Decompilación de 13 funciones `WcaUtil` (`0x1000e1b8` a `0x1000ab60`). |
| `ev_75f60000d96a7b68a37204d8f40edeaa7fb1f46d55f7ac20bdc89e5713782947` | `get_call_graph` | `wixca.dll` (`b9e73e5a...`) | Grafo de llamadas forward desde `0x10001eae`. |
| `ev_072f7d1aa067fdba24c6251a11c7949ae34d9aba9c1b9584a2a438dd3401206c` | `binary_overview` | `uica.dll` (`0fd225cb...`) | PE32 x86, Base `0x10000000`, 418 procedimientos, 377 cadenas. |
| `ev_6b157c1a3da40f141f73a22a7dde9e08290e6c2335ab199b33d6d08c71b3a840` | `batch_decompile` | `uica.dll` (`0fd225cb...`) | Decompilación de `ValidatePath` (`0x100010fe`) y `PrintEula` (`0x10001a66`). |
| `ev_ab3d8cce6216829df3b4106402553bac6f835c957498c8fa64359247c0f12396` | `batch_decompile` | `uica.dll` (`0fd225cb...`) | Decompilación de helpers `FUN_10001024`, `FUN_10001089` y `FUN_1000180a`. |
| `ev_dc8920da7cabaff4e00864cbc27a62bcc57b963ca7f615621d0fd6ccdb5f905a` | `get_call_graph` | `uica.dll` (`0fd225cb...`) | Grafo de llamadas forward de `ValidatePath`. |
| `ev_fef601935ff41b2f41ccfed6c69e6462a6ec8f2599001d58710ffbe101a25000` | `get_call_graph` | `uica.dll` (`0fd225cb...`) | Grafo de llamadas forward de `PrintEula`. |
