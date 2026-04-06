#include <gio/gio.h>

#if defined (__ELF__) && ( __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 6))
# define SECTION __attribute__ ((section (".gresource.opiqo"), aligned (sizeof(void *) > 8 ? sizeof(void *) : 8)))
#else
# define SECTION
#endif

static const SECTION union { const guint8 data[953]; const double alignment; void * const ptr;}  opiqo_resource_data = {
  "\107\126\141\162\151\141\156\164\000\000\000\000\000\000\000\000"
  "\030\000\000\000\254\000\000\000\000\000\000\050\005\000\000\000"
  "\000\000\000\000\001\000\000\000\001\000\000\000\003\000\000\000"
  "\004\000\000\000\324\265\002\000\377\377\377\377\254\000\000\000"
  "\001\000\114\000\260\000\000\000\264\000\000\000\302\257\211\013"
  "\000\000\000\000\264\000\000\000\004\000\114\000\270\000\000\000"
  "\274\000\000\000\145\073\152\234\004\000\000\000\274\000\000\000"
  "\006\000\114\000\304\000\000\000\310\000\000\000\344\146\067\274"
  "\002\000\000\000\310\000\000\000\011\000\166\000\330\000\000\000"
  "\253\003\000\000\256\327\074\031\001\000\000\000\253\003\000\000"
  "\010\000\114\000\264\003\000\000\270\003\000\000\057\000\000\000"
  "\001\000\000\000\143\157\155\057\004\000\000\000\157\160\151\161"
  "\157\057\000\000\003\000\000\000\157\160\151\161\157\056\143\163"
  "\163\000\000\000\000\000\000\000\303\002\000\000\000\000\000\000"
  "\057\052\040\157\160\151\161\157\056\143\163\163\012\040\052\040"
  "\101\160\160\154\151\143\141\164\151\157\156\055\167\151\144\145"
  "\040\163\164\171\154\145\163\150\145\145\164\040\154\157\141\144"
  "\145\144\040\166\151\141\040\107\122\145\163\157\165\162\143\145"
  "\056\012\040\052\040\117\166\145\162\162\151\144\145\163\040\141"
  "\162\145\040\141\160\160\154\151\145\144\040\141\146\164\145\162"
  "\040\101\144\167\141\151\164\141\040\057\040\144\145\146\141\165"
  "\154\164\040\107\124\113\064\040\164\150\145\155\145\056\012\040"
  "\052\057\012\012\167\151\156\144\157\167\040\173\012\040\040\040"
  "\040\142\141\143\153\147\162\157\165\156\144\055\143\157\154\157"
  "\162\072\040\043\061\145\061\145\062\145\073\012\040\040\040\040"
  "\143\157\154\157\162\072\040\043\143\144\144\066\146\064\073\012"
  "\175\012\012\056\160\154\165\147\151\156\055\163\154\157\164\040"
  "\173\012\040\040\040\040\142\157\162\144\145\162\072\040\061\160"
  "\170\040\163\157\154\151\144\040\043\064\065\064\067\065\141\073"
  "\012\040\040\040\040\142\157\162\144\145\162\055\162\141\144\151"
  "\165\163\072\040\066\160\170\073\012\040\040\040\040\155\141\162"
  "\147\151\156\072\040\064\160\170\073\012\040\040\040\040\142\141"
  "\143\153\147\162\157\165\156\144\055\143\157\154\157\162\072\040"
  "\043\061\070\061\070\062\065\073\012\175\012\012\056\163\154\157"
  "\164\055\150\145\141\144\145\162\040\173\012\040\040\040\040\142"
  "\141\143\153\147\162\157\165\156\144\055\143\157\154\157\162\072"
  "\040\043\063\061\063\062\064\064\073\012\040\040\040\040\142\157"
  "\162\144\145\162\055\162\141\144\151\165\163\072\040\066\160\170"
  "\040\066\160\170\040\060\040\060\073\012\040\040\040\040\160\141"
  "\144\144\151\156\147\072\040\062\160\170\040\064\160\170\073\012"
  "\175\012\012\056\163\154\157\164\055\156\141\155\145\040\173\012"
  "\040\040\040\040\146\157\156\164\055\167\145\151\147\150\164\072"
  "\040\142\157\154\144\073\012\040\040\040\040\143\157\154\157\162"
  "\072\040\043\070\071\142\064\146\141\073\012\175\012\012\056\163"
  "\164\141\164\165\163\055\154\141\142\145\154\040\173\012\040\040"
  "\040\040\146\157\156\164\055\163\164\171\154\145\072\040\151\164"
  "\141\154\151\143\073\012\040\040\040\040\143\157\154\157\162\072"
  "\040\043\141\066\141\144\143\070\073\012\040\040\040\040\146\157"
  "\156\164\055\163\151\172\145\072\040\060\056\070\065\145\155\073"
  "\012\175\012\012\056\170\162\165\156\055\154\141\142\145\154\040"
  "\173\012\040\040\040\040\143\157\154\157\162\072\040\043\146\141"
  "\142\063\070\067\073\012\040\040\040\040\146\157\156\164\055\163"
  "\151\172\145\072\040\060\056\070\065\145\155\073\012\040\040\040"
  "\040\155\141\162\147\151\156\055\162\151\147\150\164\072\040\070"
  "\160\170\073\012\175\012\012\056\160\154\165\147\151\156\055\156"
  "\141\155\145\040\173\012\040\040\040\040\146\157\156\164\055\167"
  "\145\151\147\150\164\072\040\142\157\154\144\073\012\040\040\040"
  "\040\143\157\154\157\162\072\040\043\143\144\144\066\146\064\073"
  "\012\175\012\000\000\050\165\165\141\171\051\144\152\163\150\141"
  "\152\151\057\000\002\000\000\000" };

static GStaticResource static_resource = { opiqo_resource_data.data, sizeof (opiqo_resource_data.data) - 1 /* nul terminator */, NULL, NULL, NULL };

G_MODULE_EXPORT
GResource *opiqo_get_resource (void);
GResource *opiqo_get_resource (void)
{
  return g_static_resource_get_resource (&static_resource);
}
/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __G_CONSTRUCTOR_H__
#define __G_CONSTRUCTOR_H__

/*
  If G_HAS_CONSTRUCTORS is true then the compiler support *both* constructors and
  destructors, in a usable way, including e.g. on library unload. If not you're on
  your own.

  Some compilers need #pragma to handle this, which does not work with macros,
  so the way you need to use this is (for constructors):

  #ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
  #pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(my_constructor)
  #endif
  G_DEFINE_CONSTRUCTOR(my_constructor)
  static void my_constructor(void) {
   ...
  }

*/

#ifndef __GTK_DOC_IGNORE__

#if  __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)

#define G_HAS_CONSTRUCTORS 1

#define G_DEFINE_CONSTRUCTOR(_func) static void __attribute__((constructor)) _func (void);
#define G_DEFINE_DESTRUCTOR(_func) static void __attribute__((destructor)) _func (void);

#elif defined (_MSC_VER)

/*
 * Only try to include gslist.h if not already included via glib.h,
 * so that items using gconstructor.h outside of GLib (such as
 * GResources) continue to build properly.
 */
#ifndef __G_LIB_H__
#include "gslist.h"
#endif

#include <stdlib.h>

#define G_HAS_CONSTRUCTORS 1

/* We do some weird things to avoid the constructors being optimized
 * away on VS2015 if WholeProgramOptimization is enabled. First we
 * make a reference to the array from the wrapper to make sure its
 * references. Then we use a pragma to make sure the wrapper function
 * symbol is always included at the link stage. Also, the symbols
 * need to be extern (but not dllexport), even though they are not
 * really used from another object file.
 */

/* We need to account for differences between the mangling of symbols
 * for x86 and x64/ARM/ARM64 programs, as symbols on x86 are prefixed
 * with an underscore but symbols on x64/ARM/ARM64 are not.
 */
#ifdef _M_IX86
#define G_MSVC_SYMBOL_PREFIX "_"
#else
#define G_MSVC_SYMBOL_PREFIX ""
#endif

#define G_DEFINE_CONSTRUCTOR(_func) G_MSVC_CTOR (_func, G_MSVC_SYMBOL_PREFIX)
#define G_DEFINE_DESTRUCTOR(_func) G_MSVC_DTOR (_func, G_MSVC_SYMBOL_PREFIX)

#define G_MSVC_CTOR(_func,_sym_prefix) \
  static void _func(void); \
  extern int (* _array ## _func)(void);              \
  int _func ## _wrapper(void);              \
  int _func ## _wrapper(void) { _func(); g_slist_find (NULL,  _array ## _func); return 0; } \
  __pragma(comment(linker,"/include:" _sym_prefix # _func "_wrapper")) \
  __pragma(section(".CRT$XCU",read)) \
  __declspec(allocate(".CRT$XCU")) int (* _array ## _func)(void) = _func ## _wrapper;

#define G_MSVC_DTOR(_func,_sym_prefix) \
  static void _func(void); \
  extern int (* _array ## _func)(void);              \
  int _func ## _constructor(void);              \
  int _func ## _constructor(void) { atexit (_func); g_slist_find (NULL,  _array ## _func); return 0; } \
   __pragma(comment(linker,"/include:" _sym_prefix # _func "_constructor")) \
  __pragma(section(".CRT$XCU",read)) \
  __declspec(allocate(".CRT$XCU")) int (* _array ## _func)(void) = _func ## _constructor;

#elif defined(__SUNPRO_C)

/* This is not tested, but i believe it should work, based on:
 * http://opensource.apple.com/source/OpenSSL098/OpenSSL098-35/src/fips/fips_premain.c
 */

#define G_HAS_CONSTRUCTORS 1

#define G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA 1
#define G_DEFINE_DESTRUCTOR_NEEDS_PRAGMA 1

#define G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(_func) \
  init(_func)
#define G_DEFINE_CONSTRUCTOR(_func) \
  static void _func(void);

#define G_DEFINE_DESTRUCTOR_PRAGMA_ARGS(_func) \
  fini(_func)
#define G_DEFINE_DESTRUCTOR(_func) \
  static void _func(void);

#else

/* constructors not supported for this compiler */

#endif

#endif /* __GTK_DOC_IGNORE__ */
#endif /* __G_CONSTRUCTOR_H__ */

#ifdef G_HAS_CONSTRUCTORS

#ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(opiqoresource_constructor)
#endif
G_DEFINE_CONSTRUCTOR(opiqoresource_constructor)
#ifdef G_DEFINE_DESTRUCTOR_NEEDS_PRAGMA
#pragma G_DEFINE_DESTRUCTOR_PRAGMA_ARGS(opiqoresource_destructor)
#endif
G_DEFINE_DESTRUCTOR(opiqoresource_destructor)

#else
#warning "Constructor not supported on this compiler, linking in resources will not work"
#endif

static void opiqoresource_constructor (void)
{
  g_static_resource_init (&static_resource);
}

static void opiqoresource_destructor (void)
{
  g_static_resource_fini (&static_resource);
}
