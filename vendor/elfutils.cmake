cmake_minimum_required(VERSION 3.10)

include(CheckSymbolExists)

set(PACKAGE_NAME elfutils)
set(PACKAGE_URL http://elfutils.org/)
set(PACKAGE_VERSION 0.177)
set(USE_LOCKS FALSE)

# TODO(strager): Turn the following constants into configure-time checks.
set(CHECK_UNDEFINED FALSE)
set(HAVE_FALLTHROUGH TRUE)
set(HAVE_GCC_STRUCT FALSE)
set(HAVE_VISIBILITY FALSE)

set(OLD_CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS})
set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
check_symbol_exists(mempcpy string.h HAVE_DECL_MEMPCPY)
check_symbol_exists(memrchr string.h HAVE_DECL_MEMRCHR)
check_symbol_exists(rawmemrchr string.h HAVE_DECL_RAWMEMRCHR)
set(CMAKE_REQUIRED_DEFINITIONS ${OLD_CMAKE_REQUIRED_DEFINITIONS})

check_symbol_exists(powerof2 sys/param.h HAVE_DECL_POWEROF2)

add_library(elfutils_common INTERFACE)
configure_file(elfutils-config.h.in config.h)
target_compile_definitions(elfutils_common INTERFACE _GNU_SOURCE HAVE_CONFIG_H PIC)
target_include_directories(elfutils_common INTERFACE ${CMAKE_CURRENT_BINARY_DIR})

add_library(elfutils_lib INTERFACE)
target_include_directories(elfutils_lib INTERFACE elfutils/lib)

add_library(
  elfutils_elf
  elfutils/libelf/elf32_checksum.c
  elfutils/libelf/elf32_fsize.c
  elfutils/libelf/elf32_getchdr.c
  elfutils/libelf/elf32_getehdr.c
  elfutils/libelf/elf32_getphdr.c
  elfutils/libelf/elf32_getshdr.c
  elfutils/libelf/elf32_newehdr.c
  elfutils/libelf/elf32_newphdr.c
  elfutils/libelf/elf32_offscn.c
  elfutils/libelf/elf32_updatefile.c
  elfutils/libelf/elf32_updatenull.c
  elfutils/libelf/elf32_xlatetof.c
  elfutils/libelf/elf32_xlatetom.c
  elfutils/libelf/elf64_checksum.c
  elfutils/libelf/elf64_fsize.c
  elfutils/libelf/elf64_getchdr.c
  elfutils/libelf/elf64_getehdr.c
  elfutils/libelf/elf64_getphdr.c
  elfutils/libelf/elf64_getshdr.c
  elfutils/libelf/elf64_newehdr.c
  elfutils/libelf/elf64_newphdr.c
  elfutils/libelf/elf64_offscn.c
  elfutils/libelf/elf64_updatefile.c
  elfutils/libelf/elf64_updatenull.c
  elfutils/libelf/elf64_xlatetof.c
  elfutils/libelf/elf64_xlatetom.c
  elfutils/libelf/elf_begin.c
  elfutils/libelf/elf_clone.c
  elfutils/libelf/elf_cntl.c
  elfutils/libelf/elf_compress.c
  elfutils/libelf/elf_compress_gnu.c
  elfutils/libelf/elf_end.c
  elfutils/libelf/elf_error.c
  elfutils/libelf/elf_fill.c
  elfutils/libelf/elf_flagdata.c
  elfutils/libelf/elf_flagehdr.c
  elfutils/libelf/elf_flagelf.c
  elfutils/libelf/elf_flagphdr.c
  elfutils/libelf/elf_flagscn.c
  elfutils/libelf/elf_flagshdr.c
  elfutils/libelf/elf_getarhdr.c
  elfutils/libelf/elf_getaroff.c
  elfutils/libelf/elf_getarsym.c
  elfutils/libelf/elf_getbase.c
  elfutils/libelf/elf_getdata.c
  elfutils/libelf/elf_getdata_rawchunk.c
  elfutils/libelf/elf_getident.c
  elfutils/libelf/elf_getphdrnum.c
  elfutils/libelf/elf_getscn.c
  elfutils/libelf/elf_getshdrnum.c
  elfutils/libelf/elf_getshdrstrndx.c
  elfutils/libelf/elf_gnu_hash.c
  elfutils/libelf/elf_hash.c
  elfutils/libelf/elf_kind.c
  elfutils/libelf/elf_memory.c
  elfutils/libelf/elf_ndxscn.c
  elfutils/libelf/elf_newdata.c
  elfutils/libelf/elf_newscn.c
  elfutils/libelf/elf_next.c
  elfutils/libelf/elf_nextscn.c
  elfutils/libelf/elf_rand.c
  elfutils/libelf/elf_rawdata.c
  elfutils/libelf/elf_rawfile.c
  elfutils/libelf/elf_readall.c
  elfutils/libelf/elf_scnshndx.c
  elfutils/libelf/elf_strptr.c
  elfutils/libelf/elf_update.c
  elfutils/libelf/elf_version.c
  elfutils/libelf/gelf_checksum.c
  elfutils/libelf/gelf_fsize.c
  elfutils/libelf/gelf_getauxv.c
  elfutils/libelf/gelf_getchdr.c
  elfutils/libelf/gelf_getclass.c
  elfutils/libelf/gelf_getdyn.c
  elfutils/libelf/gelf_getehdr.c
  elfutils/libelf/gelf_getlib.c
  elfutils/libelf/gelf_getmove.c
  elfutils/libelf/gelf_getnote.c
  elfutils/libelf/gelf_getphdr.c
  elfutils/libelf/gelf_getrel.c
  elfutils/libelf/gelf_getrela.c
  elfutils/libelf/gelf_getshdr.c
  elfutils/libelf/gelf_getsym.c
  elfutils/libelf/gelf_getsyminfo.c
  elfutils/libelf/gelf_getsymshndx.c
  elfutils/libelf/gelf_getverdaux.c
  elfutils/libelf/gelf_getverdef.c
  elfutils/libelf/gelf_getvernaux.c
  elfutils/libelf/gelf_getverneed.c
  elfutils/libelf/gelf_getversym.c
  elfutils/libelf/gelf_newehdr.c
  elfutils/libelf/gelf_newphdr.c
  elfutils/libelf/gelf_offscn.c
  elfutils/libelf/gelf_update_auxv.c
  elfutils/libelf/gelf_update_dyn.c
  elfutils/libelf/gelf_update_ehdr.c
  elfutils/libelf/gelf_update_lib.c
  elfutils/libelf/gelf_update_move.c
  elfutils/libelf/gelf_update_phdr.c
  elfutils/libelf/gelf_update_rel.c
  elfutils/libelf/gelf_update_rela.c
  elfutils/libelf/gelf_update_shdr.c
  elfutils/libelf/gelf_update_sym.c
  elfutils/libelf/gelf_update_syminfo.c
  elfutils/libelf/gelf_update_symshndx.c
  elfutils/libelf/gelf_update_verdaux.c
  elfutils/libelf/gelf_update_verdef.c
  elfutils/libelf/gelf_update_vernaux.c
  elfutils/libelf/gelf_update_verneed.c
  elfutils/libelf/gelf_update_versym.c
  elfutils/libelf/gelf_xlate.c
  elfutils/libelf/gelf_xlatetof.c
  elfutils/libelf/gelf_xlatetom.c
  elfutils/libelf/libelf_crc32.c
  elfutils/libelf/libelf_next_prime.c
  elfutils/libelf/nlist.c
)
target_compile_options(elfutils_elf PRIVATE -Wno-missing-declarations)
target_include_directories(
  elfutils_elf
  INTERFACE
  elfutils
  PUBLIC
  elfutils/libelf
)
target_link_libraries(
  elfutils_elf
  PRIVATE
  elfutils_common
  elfutils_lib
  zlib
)

add_library(elf ALIAS elfutils_elf)
