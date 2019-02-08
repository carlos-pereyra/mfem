// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.

#include "../general/okina.hpp"

#include <bitset>
#include <cassert>

#include "/home/camier1/home/okstk/stk.hpp"

namespace mfem
{
// *****************************************************************************
/*void* MMNew::Allocate(size_t count) {
   void* result = nullptr;
   const auto alloc_failed = posix_memalign(&result, ALIGNMENT, count);
   if (alloc_failed)  throw ::std::bad_alloc();
   return result;
   }*/

// *****************************************************************************
// * Tests if ptr is a known address
// *****************************************************************************
static bool Known(const mm::ledger &maps, const void *ptr)
{
   const mm::memory_map::const_iterator found = maps.memories.find(ptr);
   const bool known = found != maps.memories.end();
   if (known) { return true; }
   return false;
}

// *****************************************************************************
bool mm::IsInMM(const void *ptr){
   assert(ptr);
   return Known(maps,ptr);
}

// *****************************************************************************
void mm::Dump(const void *ptr){
   const bool known = Known(maps, ptr);
   assert(known);
   const memory &mem = maps.memories.at(ptr);
   printf("\nmem : %s:%d, func: %s @%p\n", mem.filename, mem.lineno, mem.function, mem.h_ptr);
   fflush(0);
}

// *****************************************************************************
// * Looks if ptr is an alias of one memory
// *****************************************************************************
static const void* IsAlias(const mm::ledger &maps, const void *ptr)
{
   MFEM_ASSERT(!Known(maps, ptr), "Ptr is an already known address!");
   for (mm::memory_map::const_iterator mem = maps.memories.begin();
        mem != maps.memories.end(); mem++)
   {
      const void *b_ptr = mem->first;
      if (b_ptr > ptr) { continue; }
      const void *end = (char*)b_ptr + mem->second.bytes;
      if (ptr < end) { return b_ptr; }
   }
   return NULL;
}

// *****************************************************************************
static const void* InsertAlias(mm::ledger &maps,
                               const void *base,
                               const void *ptr)
{
   mm::memory &mem = maps.memories.at(base);
   const size_t offset = (char *)ptr - (char *)base;
   const mm::alias *alias = new mm::alias{&mem, offset};
   dbg("\033[33m%p < (\033[37m%ld) < \033[33m%p", base, offset, ptr);
   maps.aliases.emplace(ptr, alias);
#ifdef MFEM_DEBUG_MM
   {
      mem.aliases.sort();
      for (const mm::alias *a : mem.aliases)
      {
         if (a->mem == &mem )
         {
            assert(a->offset != offset);
         }
      }
   }
#endif // MFEM_DEBUG
   mem.aliases.push_back(alias);
   return ptr;
}

// *****************************************************************************
// * Tests if ptr is an alias address
// *****************************************************************************
static bool Alias(mm::ledger &maps, const void *ptr)
{
   const mm::alias_map::const_iterator found = maps.aliases.find(ptr);
   const bool alias = found != maps.aliases.end();
   if (alias) { return true; }
   const void *base = IsAlias(maps, ptr);
   if (!base) { return false; }
   InsertAlias(maps, base, ptr);
   return true;
}

// *****************************************************************************
static void DumpMode(void)
{
   static bool env_ini = false;
   static bool env_dbg = false;
   if (!env_ini) { env_dbg = getenv("DBG"); env_ini = true; }
   if (!env_dbg) { return; }
   static std::bitset<9+1> mode;
   std::bitset<9+1> cfg;
   cfg.set(config::usingMM()?9:0);
   cfg.set(config::gpuHasBeenEnabled()?8:0);
   cfg.set(config::gpuEnabled()?7:0);
   cfg.set(config::gpuDisabled()?6:0);
   cfg.set(config::usingCpu()?5:0);
   cfg.set(config::usingGpu()?4:0);
   cfg.set(config::usingPA()?3:0);
   cfg.set(config::usingCuda()?2:0);
   cfg.set(config::usingOcca()?1:0);
   cfg>>=1;
   if (cfg==mode) { return; }
   mode=cfg;
   dbg("\033[1K\r[0x%x] %sMM %sHasBeenEnabled %sEnabled %sDisabled \
%sCPU %sGPU %sPA %sCUDA %sOCCA", mode.to_ulong(),
       config::usingMM()?"\033[32m":"\033[31m",
       config::gpuHasBeenEnabled()?"\033[32m":"\033[31m",
       config::gpuEnabled()?"\033[32m":"\033[31m",
       config::gpuDisabled()?"\033[32m":"\033[31m",
       config::usingCpu()?"\033[32m":"\033[31m",
       config::usingGpu()?"\033[32m":"\033[31m",
       config::usingPA()?"\033[32m":"\033[31m",
       config::usingCuda()?"\033[32m":"\033[31m",
       config::usingOcca()?"\033[32m":"\033[31m");
}

// *****************************************************************************
static inline bool MmGpuFilter(void)
{
   if (!config::usingMM()) { return true; }
   if (config::gpuDisabled()) { return true; }
   return false;
}

// *****************************************************************************
static inline bool MmGpuIniFilter(void)
{
   if (MmGpuFilter()) { return true; }
   if (!config::gpuHasBeenEnabled()) { return true; }
   return false;
}

// *****************************************************************************
// * Adds an address
// *****************************************************************************
void* mm::Insert(void *ptr, const size_t bytes,
                char const* const filename, int const lineno,
                char const* const function)
{
   //stk();
   //dbg("\033[33m%s:%d:%s", filename, lineno, function);
   //DumpMode();
   if (MmGpuFilter()) { return ptr; }
   //if (bytes==0) { return ptr; }
   const bool known = Known(maps, ptr);
   if (known) {
      const memory &mem = maps.memories.at(ptr);
      //if (mem.bytes == bytes) return ptr;
      printf("\nthis: %s:%d, func: %s @%p", filename, lineno, function, ptr);
      printf("\nmem : %s:%d, func: %s @%p", mem.filename, mem.lineno, mem.function, mem.h_ptr);
      printf("\nmem.bytes=%ld, bytes=%ld", mem.bytes, bytes);
      fflush(0);
      //BUILTIN_TRAP;
      mfem_error("unwind?");
      for(int k=0;k<1024*1024*1024;k++) ((double*)ptr)[k]=0.0;
      BUILTIN_TRAP;
      mfem_error("Trying to insert an already known pointer!");
   }
   MFEM_ASSERT(!known, "Trying to add an already known pointer!");
   //dbg("\033[33m%p \033[35m(%ldb)", ptr, bytes);
   //if (ptr > (void*)0xffffffff){ BUILTIN_TRAP; }
   DumpMode();
   maps.memories.emplace(ptr, memory(ptr, bytes, filename, lineno, function));
   return ptr;
}

// *****************************************************************************
// * Remove the address from the map, as well as all the address' aliases
// *****************************************************************************
void *mm::Erase(void *ptr, char const* const filename, int const lineno,
                char const* const function)
{
   //stk();
   //dbg("\033[31m%s:%d:%s", filename, lineno, function);
   if (MmGpuFilter()) { return ptr; }
   const bool known = Known(maps, ptr);
   if (!known) {
      if (config::usingGpu())
      {
         mfem_error("Trying to erase a non-MM pointer!");
      }else{
         //printf("\nthis: %s:%d, func: %s @%p", filename, lineno, function, ptr);
         //mfem_error("unwind?");
         // Even if don't know it, it's OK on CPU-only
         //for(int k=0;k<1024*1024;k++) ((double*)ptr)[k]=0.0;
         //BUILTIN_TRAP;
         //mfem_error("CPU, but trying to erase a non-MM pointer!");
      }
      return ptr;
   }
   MFEM_ASSERT(known, "Trying to erase a non-MM pointer!");
   memory &mem = maps.memories.at(ptr);
   //if (mem.bytes==0) { return ptr; }
   //dbg("\033[31m %p \033[35m(%ldb)", ptr, mem.bytes);
   for (const alias* const alias : mem.aliases)
   {
      maps.aliases.erase(alias);
   }
   mem.aliases.clear();
   maps.memories.erase(ptr);
   return ptr;
}

// *****************************************************************************
// * Turn a known address to the right host or device one
// * Alloc, Push or Pull it if required
// *****************************************************************************
static void* PtrKnown(mm::ledger &maps, void *ptr)
{
   mm::memory &base = maps.memories.at(ptr);
   const bool host = base.host;
   const bool device = !host;
   const size_t bytes = base.bytes;
   const bool gpu = config::usingGpu();
   if (host && !gpu) { return ptr; }
   if (!base.d_ptr) { cuMemAlloc(&base.d_ptr, bytes); }
   if (device &&  gpu) { return base.d_ptr; }
   if (device && !gpu) // Pull
   {
      cuMemcpyDtoH(ptr, base.d_ptr, bytes);
      base.host = true;
      return ptr;
   }
   // Push
   assert(host && gpu);
   cuMemcpyHtoD(base.d_ptr, ptr, bytes);
   base.host = false;
   return base.d_ptr;
}

// *****************************************************************************
// * Turn an alias to the right host or device one
// * Alloc, Push or Pull it if required
// *****************************************************************************
static void* PtrAlias(mm::ledger &maps, void *ptr)
{
   const bool gpu = config::usingGpu();
   const mm::alias *alias = maps.aliases.at(ptr);
   const mm::memory *base = alias->mem;
   const bool host = base->host;
   const bool device = !base->host;
   const size_t bytes = base->bytes;
   if (host && !gpu) { return ptr; }
   if (!base->d_ptr) { cuMemAlloc(&alias->mem->d_ptr, bytes); }
   void *a_ptr = (char*)base->d_ptr + alias->offset;
   if (device && gpu) { return a_ptr; }
   if (device && !gpu) // Pull
   {
      assert(base->d_ptr);
      cuMemcpyDtoH(base->h_ptr, base->d_ptr, bytes);
      alias->mem->host = true;
      return ptr;
   }
   // Push
   assert(host && gpu);
   cuMemcpyHtoD(base->d_ptr, base->h_ptr, bytes);
   alias->mem->host = false;
   return a_ptr;
}

// *****************************************************************************
// * Turn an address to the right host or device one
// *****************************************************************************
void* mm::Ptr(void *ptr)
{
   if (MmGpuIniFilter()) { return ptr; }
   if (Known(maps, ptr)) { return PtrKnown(maps, ptr); }
   if (Alias(maps, ptr)) { return PtrAlias(maps, ptr); }
   /*if (config::usingGpu())*/ { mfem_error("Unknown pointer!"); }
   return ptr;
}

// *****************************************************************************
const void* mm::Ptr(const void *ptr) { return (const void *) Ptr((void *)ptr); }

// *****************************************************************************
static OccaMemory occaMemory(mm::ledger &maps, const void *ptr)
{
   OccaDevice occaDevice = config::GetOccaDevice();
   if (!config::usingMM())
   {
      OccaMemory o_ptr = occaWrapMemory(occaDevice, (void *)ptr, 0);
      return o_ptr;
   }
   const bool known = Known(maps, ptr);
   if (!known) { mfem_error("occaMemory"); }
   MFEM_ASSERT(known, "Unknown address!");
   mm::memory &base = maps.memories.at(ptr);
   const size_t bytes = base.bytes;
   const bool gpu = config::usingGpu();
   const bool occa = config::usingOcca();
   MFEM_ASSERT(occa, "Using OCCA memory without OCCA mode!");
   if (!base.d_ptr)
   {
      base.host = false; // This address is no more on the host
      if (gpu)
      {
         cuMemAlloc(&base.d_ptr, bytes);
         void *stream = config::Stream();
         cuMemcpyHtoDAsync(base.d_ptr, base.h_ptr, bytes, stream);
      }
      else
      {
         base.o_ptr = occaDeviceMalloc(occaDevice, bytes);
         base.d_ptr = occaMemoryPtr(base.o_ptr);
         occaCopyFrom(base.o_ptr, base.h_ptr);
      }
   }
   if (gpu)
   {
      return occaWrapMemory(occaDevice, base.d_ptr, bytes);
   }
   return base.o_ptr;
}

// *****************************************************************************
OccaMemory mm::Memory(const void *ptr) { return occaMemory(maps, ptr); }

// *****************************************************************************
static void PushKnown(mm::ledger &maps, const void *ptr, const size_t bytes)
{
   mm::memory &base = maps.memories.at(ptr);
   if (!base.d_ptr) { cuMemAlloc(&base.d_ptr, base.bytes); }
   cuMemcpyHtoD(base.d_ptr, ptr, bytes == 0 ? base.bytes : bytes);
}

// *****************************************************************************
static void PushAlias(const mm::ledger &maps, const void *ptr,
                      const size_t bytes)
{
   const mm::alias *alias = maps.aliases.at(ptr);
   cuMemcpyHtoD((char*)alias->mem->d_ptr + alias->offset, ptr, bytes);
}

// *****************************************************************************
void mm::Push(const void *ptr, const size_t bytes)
{
   if (MmGpuIniFilter()) { return; }
   if (Known(maps, ptr)) { return PushKnown(maps, ptr, bytes); }
   if (Alias(maps, ptr)) { return PushAlias(maps, ptr, bytes); }
   assert(!config::usingOcca());
   /*if (config::usingGpu())*/ { mfem_error("Unknown address!"); }
}

// *****************************************************************************
static void PullKnown(const mm::ledger &maps, const void *ptr,
                      const size_t bytes)
{
   const mm::memory &base = maps.memories.at(ptr);
   const bool host = base.host;
   if (host) { return; }
   cuMemcpyDtoH(base.h_ptr, base.d_ptr, bytes == 0 ? base.bytes : bytes);
}

// *****************************************************************************
static void PullAlias(const mm::ledger &maps, const void *ptr,
                      const size_t bytes)
{
   const mm::alias *alias = maps.aliases.at(ptr);
   const bool host = alias->mem->host;
   if (host) { return; }
   cuMemcpyDtoH((void *)ptr, (char*)alias->mem->d_ptr + alias->offset, bytes);
}

// *****************************************************************************
void mm::Pull(const void *ptr, const size_t bytes)
{
   if (MmGpuIniFilter()) { return; }
   if (Known(maps, ptr)) { return PullKnown(maps, ptr, bytes); }
   if (Alias(maps, ptr)) { return PullAlias(maps, ptr, bytes); }
   assert(!config::usingOcca());
   /*if (config::usingGpu())*/ { mfem_error("Unknown address!"); }
}

// *****************************************************************************
// * Data will be pushed/pulled before the copy happens on the H or the D
// *****************************************************************************
static void* d2d(void *dst, const void *src, const size_t bytes,
                 const bool async)
{
   const bool cpu = config::usingCpu();
   if (cpu) { return std::memcpy(dst, src, bytes); }
   assert(false);
   GET_PTR(src);
   GET_PTR(dst);
   if (!async) { return cuMemcpyDtoD(d_dst, (void *)d_src, bytes); }
   return cuMemcpyDtoDAsync(d_dst, (void *)d_src, bytes, config::Stream());
}

// *****************************************************************************
void* mm::memcpy(void *dst, const void *src, const size_t bytes,
                 const bool async)
{
   if (bytes == 0)
   {
      return dst;
   }
   else
   {
      return d2d(dst, src, bytes, async);
   }
}

} // namespace mfem