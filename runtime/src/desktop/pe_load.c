// pe_load.c — loader de PE do host (desktop e, depois, Android).
//
// Modelo do pcrecomp: mapeamento IDENTIDADE — a imagem do guest ocupa a
// memoria no proprio endereco preferido (ImageBase), e o runtime acessa
// rd64(a) == *(uint64_t*)a. Este loader:
//   1. reserva SizeOfImage bytes no ImageBase
//   2. copia headers + secoes
//   3. aplica relocacoes DIR64 (delta = base real - base preferida)
//   4. os slots da IAT sao preenchidos depois, pelo hle.c, com sentinelas
//      HLE_SENTINEL(i) que o dispatch traduz para os shims do host.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#define DOS_MAGIC 0x5A4D
#define NT_SIG 0x00004550
#define IMAGE_DIRECTORY_ENTRY_BASERELOC 5
#define IMAGE_REL_BASED_DIR64 10

typedef struct {
    uint16_t e_magic;
    uint8_t _pad[58];
    int32_t e_lfanew;
} dos_hdr_t;

typedef struct {
    uint16_t Machine, NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable, NumberOfSymbols;
    uint16_t SizeOfOptionalHeader, Characteristics;
} file_hdr_t;

typedef struct {
    uint16_t Magic;
    uint8_t MajorLinkerVersion, MinorLinkerVersion;
    uint32_t SizeOfCode, SizeOfInitializedData, SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment, FileAlignment;
    uint16_t MajorOperatingSystemVersion, MinorOperatingSystemVersion;
    uint16_t MajorImageVersion, MinorImageVersion;
    uint16_t MajorSubsystemVersion, MinorSubsystemVersion;
    uint32_t Win32VersionValue, SizeOfImage, SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem, DllCharacteristics;
    uint64_t SizeOfStackReserve, SizeOfStackCommit;
    uint64_t SizeOfHeapReserve, SizeOfHeapCommit;
    uint32_t LoaderFlags, NumberOfRvaAndSizes;
    struct { uint32_t VirtualAddress, Size; } DataDirectory[16];
} opt_hdr_t;

typedef struct {
    uint32_t Signature;
    file_hdr_t FileHeader;
    opt_hdr_t OptionalHeader;
} nt_hdr_t;

typedef struct {
    char Name[8];
    uint32_t VirtualSize, VirtualAddress, SizeOfRawData, PointerToRawData;
    uint32_t PointerToRelocations, PointerToLinenumbers;
    uint16_t NumberOfRelocations, NumberOfLinenumbers;
    uint32_t Characteristics;
} sec_hdr_t;

// Reserva/mapeia a regiao do guest no endereco preferido.
static void* sb_reserve_at(uint64_t base, size_t size) {
#ifdef _WIN32
    void* p = VirtualAlloc((LPVOID)(uintptr_t)base, size,
                           MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    return p;
#else
    void* p = mmap((void*)(uintptr_t)base, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    return p == MAP_FAILED ? NULL : p;
#endif
}

// Aplica relocacoes DIR64: delta = base real - base preferida.
static void apply_relocs(uint8_t* base, uint64_t delta, uint32_t rva, uint32_t size) {
    if (!rva || !size) return;
    uint8_t* p = base + rva;
    uint8_t* end = p + size;
    while (p + 8 <= end) {
        uint32_t page_rva = *(uint32_t*)p;
        uint32_t block_size = *(uint32_t*)(p + 4);
        if (!block_size) break;
        int n = (block_size - 8) / 2;
        for (int i = 0; i < n; ++i) {
            uint16_t e = *(uint16_t*)(p + 8 + 2 * i);
            uint16_t type = e >> 12;
            uint16_t off = e & 0xFFF;
            if (type == IMAGE_REL_BASED_DIR64) {
                uint64_t* slot = (uint64_t*)(base + page_rva + off);
                *slot += delta;
            }
        }
        p += block_size;
    }
}

// ---- exports -----------------------------------------------------------------
// Preenche `out` (name -> endereco no guest) com os exports do PE.
// Os nomes apontam para a imagem mapeada (estavel após o load).
typedef struct { const char* name; uint64_t va; } sb_export_t;

#define IMAGE_DIRECTORY_ENTRY_EXPORT 0

int sb_pe_exports(const char* path, sb_export_t* out, int cap) {
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* buf = (uint8_t*)malloc(fsz);
    if (!buf || fread(buf, 1, fsz, f) != (size_t)fsz) { fclose(f); return 0; }
    fclose(f);

    nt_hdr_t* nt = (nt_hdr_t*)(buf + ((dos_hdr_t*)buf)->e_lfanew);
    uint32_t rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    if (!rva) { free(buf); return 0; }

    uint64_t preferred = nt->OptionalHeader.ImageBase;
    // acha a secao que contem o export dir
    sec_hdr_t* secs = (sec_hdr_t*)((uint8_t*)&nt->OptionalHeader
                                   + nt->FileHeader.SizeOfOptionalHeader);
    uint8_t* eat = NULL;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (rva >= secs[i].VirtualAddress
            && rva < secs[i].VirtualAddress + secs[i].VirtualSize) {
            eat = buf + secs[i].PointerToRawData + (rva - secs[i].VirtualAddress);
            break;
        }
    }
    if (!eat) { free(buf); return 0; }

    uint32_t nNames = *(uint32_t*)(eat + 24);
    uint32_t funcsRVA = *(uint32_t*)(eat + 28);
    uint32_t namesRVA = *(uint32_t*)(eat + 32);
    uint32_t ordsRVA = *(uint32_t*)(eat + 36);

    // converte RVA -> offset no arquivo (mesma logica de secao)
    #define RVA2OFF(_rva) ({ uint32_t _r = (_rva); uint8_t* _p = NULL; \
        for (int _i = 0; _i < nt->FileHeader.NumberOfSections; ++_i) { \
            if (_r >= secs[_i].VirtualAddress && \
                _r < secs[_i].VirtualAddress + secs[_i].VirtualSize) { \
                _p = buf + secs[_i].PointerToRawData + (_r - secs[_i].VirtualAddress); \
                break; } } _p; })

    uint32_t* funcs = (uint32_t*)RVA2OFF(funcsRVA);
    uint32_t* names = (uint32_t*)RVA2OFF(namesRVA);
    uint16_t* ords = (uint16_t*)RVA2OFF(ordsRVA);
    if (!funcs || !names || !ords) { free(buf); return 0; }

    int n = 0;
    for (uint32_t i = 0; i < nNames && n < cap; ++i) {
        char* nm = (char*)RVA2OFF(names[i]);
        if (!nm) continue;
        out[n].name = nm;                          // estavel (imagem mapeada)
        out[n].va = preferred + funcs[ords[i]];    // endereco no guest
        ++n;
    }
    #undef RVA2OFF
    free(buf);
    return n;
}

// Carrega o PE e devolve o endereco do entry point (0 = falha).
uint64_t sb_load_pe(const char* path, uint64_t* out_base, uint32_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) { printf("[loader] nao abri %s\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* buf = (uint8_t*)malloc(fsz);
    if (!buf || fread(buf, 1, fsz, f) != (size_t)fsz) { fclose(f); return 0; }
    fclose(f);

    if (*(uint16_t*)buf != DOS_MAGIC) { printf("[loader] nao eh MZ\n"); return 0; }
    nt_hdr_t* nt = (nt_hdr_t*)(buf + ((dos_hdr_t*)buf)->e_lfanew);
    if (nt->Signature != NT_SIG) { printf("[loader] nao eh PE\n"); return 0; }

    opt_hdr_t* oh = &nt->OptionalHeader;
    uint64_t preferred = oh->ImageBase;
    uint32_t size_of_image = oh->SizeOfImage;

    void* mapped = sb_reserve_at(preferred, size_of_image);
    if (!mapped) {
        printf("[loader] nao reservei %zu bytes em 0x%llx\n",
               (size_t)size_of_image, (unsigned long long)preferred);
        return 0;
    }
    memset(mapped, 0, size_of_image);

    // headers
    memcpy(mapped, buf, oh->SizeOfHeaders);

    // secoes: VA do destino, RAW de origem
    sec_hdr_t* secs = (sec_hdr_t*)((uint8_t*)oh + nt->FileHeader.SizeOfOptionalHeader);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        uint32_t va = secs[i].VirtualAddress;
        uint32_t vsz = secs[i].VirtualSize;
        uint32_t rsz = secs[i].SizeOfRawData;
        uint32_t rva = secs[i].PointerToRawData;
        if (!vsz) continue;
        uint32_t n = rsz < vsz ? rsz : vsz;
        if (n) memcpy((uint8_t*)mapped + va, buf + rva, n);
    }

    // relocacoes (delta 0 quando a base preferida foi obtida)
    uint64_t delta = preferred - preferred;   // = 0; mantido explicito
    apply_relocs((uint8_t*)mapped, delta,
                 oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress,
                 oh->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);

    *out_base = preferred;
    *out_size = size_of_image;
    uint32_t entry = oh->AddressOfEntryPoint;
    free(buf);
    printf("[loader] mapeado 0x%llx (%u bytes), entry RVA 0x%x\n",
           (unsigned long long)preferred, size_of_image, entry);
    return entry ? preferred + entry : preferred;
}
