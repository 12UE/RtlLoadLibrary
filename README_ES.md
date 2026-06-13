# RtlLoadLibrary

**[English](README.md) | [中文](README_CN.md) | [Español](README_ES.md)**

Una biblioteca de carga manual de PE en memoria para Windows que permite cargar DLLs directamente desde memoria sin escribir en disco, eludiendo el mecanismo estándar de `LoadLibrary`.

## Características

- **Carga en memoria**: Carga imágenes PE desde búferes de memoria sin E/S de archivos
- **Resolución de API Set**: Resuelve automáticamente nombres de DLL virtuales `api-ms-win-*` / `ext-ms-win-*` a DLLs reales del sistema
- **Pipeline completo de reparación PE**:
  - Reparación de tabla de importaciones (IAT)
  - Reparación de importaciones de carga diferida (Delay-Load)
  - Reparación de tabla de reubicaciones
  - Ejecución de callbacks TLS
  - Reparación de tabla de exportaciones
  - Reparación de tabla de manejo de excepciones (SEH/CFG)
- **Gestión LdrLink**: Vincula módulos cargados manualmente a la lista de módulos del PEB
- **Wrapper de Loader Lock**: `RtlLockLoaderLock` / `RtlUnlockLoaderLock` para protección de carga concurrente
- **Interfaz dual ANSI/Wide**: Todas las APIs principales proporcionan versiones `A` y `W`
- **Carga asíncrona**: Carga asíncrona de DLL mediante `std::future`
- **Gestión de recursos RAII**: `GenericHandle` / `FileMap` para gestión automática de handles del kernel de Windows
- **Cifrado de cadenas**: Cifrado XOR en tiempo de compilación de cadenas sensibles para prevenir análisis estático

## Compilación

### Requisitos

- Visual Studio 2019 o posterior
- Windows SDK 10.0+
- Estándar C++17

### Compilar

1. Abrir `RtlLoadLibrary.sln`
2. Seleccionar plataforma objetivo (x86 / x64) y configuración (Debug / Release)
3. Compilar la solución

Salida: `RtlLoadLibrary.dll`

## Referencia de API

### Funciones principales de carga

```cpp
// Versión wide-char — cargar DLL por nombre de archivo
HMODULE WINAPI RtlLoadLibraryW(LPCWSTR lpLibFileName);

// Versión ANSI
HMODULE WINAPI RtlLoadLibraryA(LPCSTR lpLibFileName);

// Versión extendida — soporta handle de archivo y flags
HMODULE WINAPI RtlLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
HMODULE WINAPI RtlLoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

// Cargar desde búfer de memoria
HMODULE WINAPI RtlLoadLibraryInMemory(PVOID lpBytes, DWORD dwSize);

// Cargar desde búfer de memoria (extendido, soporta nombre de archivo y ruta completa)
HMODULE WINAPI RtlLoadLibraryInMemoryEx(
    PVOID  lpBytes,
    DWORD  dwSize,
    DWORD  dwFlags,
    LPWSTR lpFileName,
    LPWSTR lpFullName
);
```

### Consulta de módulos

```cpp
HMODULE WINAPI RtlGetModuleHandleA(LPCSTR  lpModuleName);
HMODULE WINAPI RtlGetModuleHandleW(LPCWSTR lpModuleName);
FARPROC WINAPI RtlGetProcAddress(HMODULE hModule, LPCSTR lpProcName);
```

### Sección / Punto de entrada

```cpp
PVOID WINAPI RtlGetSectionDataA(HMODULE hMod, LPCSTR  lpSecName);
PVOID WINAPI RtlGetSectionDataW(HMODULE hMod, LPCWSTR lpSecName);
PVOID WINAPI RtlGetEntryPoint(HMODULE hMod);
```

### Loader Lock

```cpp
void WINAPI RtlLockLoaderLock();
void WINAPI RtlUnlockLoaderLock();
```

### Gestión de lista LDR

```cpp
bool WINAPI AddLdrLink(const MY_LDR_DATA_TABLE_ENTRY& Entry, BOOL bInitialize);
bool WINAPI RemoveLdrLink(const UNICODE_STRING* BaseName, const UNICODE_STRING* FullPath);
```

### Funciones de utilidad

```cpp
// Carga asíncrona
std::future<HMODULE> AsyncRtlLoadLibraryA(LPCSTR lpLibFileName);

// Detección de nombre API Set
bool IsApiSetName(const std::wstring& name);

```

## Ejemplos de uso

### Carga básica de archivo

```cpp
#include "RtlLoadLibrary.h"

// Cargar user32.dll (equivalente a LoadLibraryW)
HMODULE hUser32 = RtlLoadLibraryW(L"user32.dll");

// Obtener dirección de función
auto pMessageBox = (int(WINAPI*)(HWND, LPCWSTR, LPCWSTR, UINT))
    RtlGetProcAddress(hUser32, "MessageBoxW");

pMessageBox(NULL, L"Hello", L"RtlLoadLibrary", MB_OK);
```

### Carga desde memoria

```cpp
// Leer DLL en memoria
HANDLE hFile = CreateFileW(L"mylib.dll", GENERIC_READ, FILE_SHARE_READ,
    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

DWORD dwFileSize = GetFileSize(hFile, NULL);
std::vector<BYTE> buf(dwFileSize);
ReadFile(hFile, buf.data(), dwFileSize, NULL, NULL);
CloseHandle(hFile);

// Cargar desde memoria
HMODULE hMod = RtlLoadLibraryInMemory(buf.data(), dwFileSize);

// Obtener función exportada
auto pFunc = (void(*)())RtlGetProcAddress(hMod, "MyExportedFunc");
pFunc();
```

### Carga asíncrona

```cpp
auto future = AsyncRtlLoadLibraryA("kernel32.dll");
// ... hacer otro trabajo ...
HMODULE hMod = future.get();  // Bloquear hasta que se complete la carga
```

### Carga segura entre hilos

```cpp
RtlLockLoaderLock();
HMODULE hMod1 = RtlLoadLibraryInMemory(buf1, size1);
HMODULE hMod2 = RtlLoadLibraryInMemory(buf2, size2);
RtlUnlockLoaderLock();
```

## Visión general de la arquitectura

```
RtlLoadLibrary/
├── RtlLoadLibrary.h/cpp    # Entrada de implementación de API exportada
├── RtlMemory.h/cpp          # Motor principal de carga PE (1453 líneas)
├── ApisetQuery.h/cpp        # Resolutor de API Set Schema (singleton)
├── RtlGetModuleHandle.h/cpp # Implementación personalizada de GetModuleHandle
├── RtlGetProcAddress.h/cpp  # Implementación personalizada de GetProcAddress
├── LdrLink.h/cpp            # Operaciones de lista enlazada PEB LDR
├── GlobalObject.h/cpp       # Gestión de estado global (singleton)
├── KernelStruct.h           # Definiciones de estructuras internas NT
├── Singleton.h              # Clase base de plantilla singleton
├── GenericHandle.h          # Wrapper RAII de handles
├── FileMap.h/cpp            # Wrapper RAII de mapeo de archivos
├── XorStr.h                 # Cifrado de cadenas en tiempo de compilación
├── DebugPrint.h/cpp         # Sistema de logging
├── StringUtils.h/cpp        # Conversión de caracteres wide/narrow
├── ThreadObject.h/cpp       # Wrapper de operaciones de hilos
├── EnumModule.h/cpp         # Enumeración de módulos
├── EnumThread.h/cpp         # Enumeración de hilos
├── ValidAddress.h/cpp       # Verificación de validez de direcciones
├── Exports.def              # Definiciones de símbolos exportados
├── pch.h/cpp                # Encabezado precompilado
└── main.cpp                 # Entrada de prueba (no es código de biblioteca)
```

### Flujo de carga

1. Validar encabezados PE (DOS/NT Header)
2. Calcular tamaño total de la imagen
3. Asignar memoria virtual (preferir ImageBase, respaldo ASLR)
4. Copiar datos de secciones a memoria virtual
5. Reparar tabla de importaciones (IAT) — cargar recursivamente DLLs dependientes
6. Reparar tabla de importaciones de carga diferida
7. Reparar tabla de reubicaciones (si la dirección base cambió)
8. Reparar tabla de exportaciones
9. Reparar tabla de manejo de excepciones
10. Ejecutar callbacks TLS
11. Vincular a la lista PEB LDR
12. Llamar a DllMain(DLL_PROCESS_ATTACH)
13. Liberar memoria de secciones descartables

## Notas

- Este proyecto es solo para fines educativos y de investigación sobre mecanismos de carga PE de Windows
- Las DLLs cargadas desde memoria no pueden ser encontradas por APIs estándar como `GetModuleHandle`; use el `RtlGetModuleHandle` proporcionado
- Cargar DLLs de 64 bits requiere la compilación x64; cargar DLLs de 32 bits requiere la compilación x86
- No aplicable a ensamblados administrados de .NET

## Licencia

Solo para fines educativos y de investigación.
