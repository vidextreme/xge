// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// Minimal SAL stub so DirectXMath builds outside MSVC (it includes "sal.h" unconditionally). Only on the include
// path of non-Windows builds; the annotations carry no meaning for GCC or Clang.
#pragma once

#ifndef _In_
#define _In_
#endif
#ifndef _In_reads_
#define _In_reads_(size)
#endif
#ifndef _In_reads_bytes_
#define _In_reads_bytes_(size)
#endif
#ifndef _Out_
#define _Out_
#endif
#ifndef _Out_opt_
#define _Out_opt_
#endif
#ifndef _Out_writes_
#define _Out_writes_(size)
#endif
#ifndef _Out_writes_bytes_
#define _Out_writes_bytes_(size)
#endif
#ifndef _Success_
#define _Success_(expr)
#endif
#ifndef _Analysis_assume_
#define _Analysis_assume_(expr)
#endif
#ifndef _Use_decl_annotations_
#define _Use_decl_annotations_
#endif
