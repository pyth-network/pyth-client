#![feature(prelude_import)]
#![allow(non_upper_case_globals)]
#![allow(clippy::not_unsafe_ptr_arg_deref)]
#[prelude_import]
use std::prelude::rust_2021::*;
#[macro_use]
extern crate std;
mod c_oracle_header {
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(dead_code)]
    use bytemuck::{Pod, Zeroable};
    use solana_program::pubkey::Pubkey;
    use std::mem::size_of;
    #[repr(C)]
    pub struct __IncompleteArrayField<T>(::std::marker::PhantomData<T>, [T; 0]);
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl<T: ::core::default::Default> ::core::default::Default for __IncompleteArrayField<T> {
        #[inline]
        fn default() -> __IncompleteArrayField<T> {
            __IncompleteArrayField(
                ::core::default::Default::default(),
                ::core::default::Default::default(),
            )
        }
    }
    impl<T> __IncompleteArrayField<T> {
        #[inline]
        pub const fn new() -> Self {
            __IncompleteArrayField(::std::marker::PhantomData, [])
        }
        #[inline]
        pub fn as_ptr(&self) -> *const T {
            self as *const _ as *const T
        }
        #[inline]
        pub fn as_mut_ptr(&mut self) -> *mut T {
            self as *mut _ as *mut T
        }
        #[inline]
        pub unsafe fn as_slice(&self, len: usize) -> &[T] {
            ::std::slice::from_raw_parts(self.as_ptr(), len)
        }
        #[inline]
        pub unsafe fn as_mut_slice(&mut self, len: usize) -> &mut [T] {
            ::std::slice::from_raw_parts_mut(self.as_mut_ptr(), len)
        }
    }
    impl<T> ::std::fmt::Debug for __IncompleteArrayField<T> {
        fn fmt(&self, fmt: &mut ::std::fmt::Formatter<'_>) -> ::std::fmt::Result {
            fmt.write_str("__IncompleteArrayField")
        }
    }
    pub const true_: u32 = 1;
    pub const false_: u32 = 0;
    pub const __bool_true_false_are_defined: u32 = 1;
    pub const PYTH_ORACLE_UTIL_COMPAT_STDINT_STYLE: u32 = 0;
    pub const __WORDSIZE: u32 = 64;
    pub const __DARWIN_ONLY_64_BIT_INO_T: u32 = 1;
    pub const __DARWIN_ONLY_UNIX_CONFORMANCE: u32 = 1;
    pub const __DARWIN_ONLY_VERS_1050: u32 = 1;
    pub const __DARWIN_UNIX03: u32 = 1;
    pub const __DARWIN_64_BIT_INO_T: u32 = 1;
    pub const __DARWIN_VERS_1050: u32 = 1;
    pub const __DARWIN_NON_CANCELABLE: u32 = 0;
    pub const __DARWIN_SUF_EXTSN: &[u8; 14usize] = b"$DARWIN_EXTSN\0";
    pub const __DARWIN_C_ANSI: u32 = 4096;
    pub const __DARWIN_C_FULL: u32 = 900000;
    pub const __DARWIN_C_LEVEL: u32 = 900000;
    pub const __STDC_WANT_LIB_EXT1__: u32 = 1;
    pub const __DARWIN_NO_LONG_LONG: u32 = 0;
    pub const _DARWIN_FEATURE_64_BIT_INODE: u32 = 1;
    pub const _DARWIN_FEATURE_ONLY_64_BIT_INODE: u32 = 1;
    pub const _DARWIN_FEATURE_ONLY_VERS_1050: u32 = 1;
    pub const _DARWIN_FEATURE_ONLY_UNIX_CONFORMANCE: u32 = 1;
    pub const _DARWIN_FEATURE_UNIX_CONFORMANCE: u32 = 3;
    pub const __has_ptrcheck: u32 = 0;
    pub const __PTHREAD_SIZE__: u32 = 8176;
    pub const __PTHREAD_ATTR_SIZE__: u32 = 56;
    pub const __PTHREAD_MUTEXATTR_SIZE__: u32 = 8;
    pub const __PTHREAD_MUTEX_SIZE__: u32 = 56;
    pub const __PTHREAD_CONDATTR_SIZE__: u32 = 8;
    pub const __PTHREAD_COND_SIZE__: u32 = 40;
    pub const __PTHREAD_ONCE_SIZE__: u32 = 8;
    pub const __PTHREAD_RWLOCK_SIZE__: u32 = 192;
    pub const __PTHREAD_RWLOCKATTR_SIZE__: u32 = 16;
    pub const INT8_MAX: u32 = 127;
    pub const INT16_MAX: u32 = 32767;
    pub const INT32_MAX: u32 = 2147483647;
    pub const INT64_MAX: u64 = 9223372036854775807;
    pub const INT8_MIN: i32 = -128;
    pub const INT16_MIN: i32 = -32768;
    pub const INT32_MIN: i32 = -2147483648;
    pub const INT64_MIN: i64 = -9223372036854775808;
    pub const UINT8_MAX: u32 = 255;
    pub const UINT16_MAX: u32 = 65535;
    pub const UINT32_MAX: u32 = 4294967295;
    pub const UINT64_MAX: i32 = -1;
    pub const INT_LEAST8_MIN: i32 = -128;
    pub const INT_LEAST16_MIN: i32 = -32768;
    pub const INT_LEAST32_MIN: i32 = -2147483648;
    pub const INT_LEAST64_MIN: i64 = -9223372036854775808;
    pub const INT_LEAST8_MAX: u32 = 127;
    pub const INT_LEAST16_MAX: u32 = 32767;
    pub const INT_LEAST32_MAX: u32 = 2147483647;
    pub const INT_LEAST64_MAX: u64 = 9223372036854775807;
    pub const UINT_LEAST8_MAX: u32 = 255;
    pub const UINT_LEAST16_MAX: u32 = 65535;
    pub const UINT_LEAST32_MAX: u32 = 4294967295;
    pub const UINT_LEAST64_MAX: i32 = -1;
    pub const INT_FAST8_MIN: i32 = -128;
    pub const INT_FAST16_MIN: i32 = -32768;
    pub const INT_FAST32_MIN: i32 = -2147483648;
    pub const INT_FAST64_MIN: i64 = -9223372036854775808;
    pub const INT_FAST8_MAX: u32 = 127;
    pub const INT_FAST16_MAX: u32 = 32767;
    pub const INT_FAST32_MAX: u32 = 2147483647;
    pub const INT_FAST64_MAX: u64 = 9223372036854775807;
    pub const UINT_FAST8_MAX: u32 = 255;
    pub const UINT_FAST16_MAX: u32 = 65535;
    pub const UINT_FAST32_MAX: u32 = 4294967295;
    pub const UINT_FAST64_MAX: i32 = -1;
    pub const INTPTR_MAX: u64 = 9223372036854775807;
    pub const INTPTR_MIN: i64 = -9223372036854775808;
    pub const UINTPTR_MAX: i32 = -1;
    pub const SIZE_MAX: i32 = -1;
    pub const RSIZE_MAX: i32 = -1;
    pub const WINT_MIN: i32 = -2147483648;
    pub const WINT_MAX: u32 = 2147483647;
    pub const SIG_ATOMIC_MIN: i32 = -2147483648;
    pub const SIG_ATOMIC_MAX: u32 = 2147483647;
    pub const PC_MAGIC: u32 = 2712847316;
    pub const PC_VERSION: u32 = 2;
    pub const PC_MAX_SEND_LATENCY: u32 = 25;
    pub const PC_PUBKEY_SIZE: u32 = 32;
    pub const PC_MAP_TABLE_SIZE: u32 = 640;
    pub const PC_COMP_SIZE: u32 = 32;
    pub const PC_MAX_NUM_DECIMALS: u32 = 8;
    pub const PC_PROD_ACC_SIZE: u32 = 512;
    pub const PC_EXP_DECAY: i32 = -9;
    pub const PC_MAX_CI_DIVISOR: u32 = 20;
    pub const PC_HEAP_START: u64 = 12884901888;
    pub const PC_PTYPE_UNKNOWN: u32 = 0;
    pub const PC_PTYPE_PRICE: u32 = 1;
    pub const PC_STATUS_UNKNOWN: u32 = 0;
    pub const PC_STATUS_TRADING: u32 = 1;
    pub const PC_STATUS_HALTED: u32 = 2;
    pub const PC_STATUS_AUCTION: u32 = 3;
    pub const PC_ACCTYPE_MAPPING: u32 = 1;
    pub const PC_ACCTYPE_PRODUCT: u32 = 2;
    pub const PC_ACCTYPE_PRICE: u32 = 3;
    pub const PC_ACCTYPE_TEST: u32 = 4;
    pub type size_t = ::std::os::raw::c_ulong;
    pub type wchar_t = ::std::os::raw::c_int;
    pub type max_align_t = f64;
    pub type int_least8_t = i8;
    pub type int_least16_t = i16;
    pub type int_least32_t = i32;
    pub type int_least64_t = i64;
    pub type uint_least8_t = u8;
    pub type uint_least16_t = u16;
    pub type uint_least32_t = u32;
    pub type uint_least64_t = u64;
    pub type int_fast8_t = i8;
    pub type int_fast16_t = i16;
    pub type int_fast32_t = i32;
    pub type int_fast64_t = i64;
    pub type uint_fast8_t = u8;
    pub type uint_fast16_t = u16;
    pub type uint_fast32_t = u32;
    pub type uint_fast64_t = u64;
    pub type __int8_t = ::std::os::raw::c_schar;
    pub type __uint8_t = ::std::os::raw::c_uchar;
    pub type __int16_t = ::std::os::raw::c_short;
    pub type __uint16_t = ::std::os::raw::c_ushort;
    pub type __int32_t = ::std::os::raw::c_int;
    pub type __uint32_t = ::std::os::raw::c_uint;
    pub type __int64_t = ::std::os::raw::c_longlong;
    pub type __uint64_t = ::std::os::raw::c_ulonglong;
    pub type __darwin_intptr_t = ::std::os::raw::c_long;
    pub type __darwin_natural_t = ::std::os::raw::c_uint;
    pub type __darwin_ct_rune_t = ::std::os::raw::c_int;
    #[repr(C)]
    pub union __mbstate_t {
        pub __mbstate8: [::std::os::raw::c_char; 128usize],
        pub _mbstateL: ::std::os::raw::c_longlong,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for __mbstate_t {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for __mbstate_t {
        #[inline]
        fn clone(&self) -> __mbstate_t {
            {
                let _: ::core::clone::AssertParamIsCopy<Self>;
                *self
            }
        }
    }
    pub type __darwin_mbstate_t = __mbstate_t;
    pub type __darwin_ptrdiff_t = ::std::os::raw::c_long;
    pub type __darwin_size_t = ::std::os::raw::c_ulong;
    pub type __darwin_va_list = __builtin_va_list;
    pub type __darwin_wchar_t = ::std::os::raw::c_int;
    pub type __darwin_rune_t = __darwin_wchar_t;
    pub type __darwin_wint_t = ::std::os::raw::c_int;
    pub type __darwin_clock_t = ::std::os::raw::c_ulong;
    pub type __darwin_socklen_t = __uint32_t;
    pub type __darwin_ssize_t = ::std::os::raw::c_long;
    pub type __darwin_time_t = ::std::os::raw::c_long;
    pub type __darwin_blkcnt_t = __int64_t;
    pub type __darwin_blksize_t = __int32_t;
    pub type __darwin_dev_t = __int32_t;
    pub type __darwin_fsblkcnt_t = ::std::os::raw::c_uint;
    pub type __darwin_fsfilcnt_t = ::std::os::raw::c_uint;
    pub type __darwin_gid_t = __uint32_t;
    pub type __darwin_id_t = __uint32_t;
    pub type __darwin_ino64_t = __uint64_t;
    pub type __darwin_ino_t = __darwin_ino64_t;
    pub type __darwin_mach_port_name_t = __darwin_natural_t;
    pub type __darwin_mach_port_t = __darwin_mach_port_name_t;
    pub type __darwin_mode_t = __uint16_t;
    pub type __darwin_off_t = __int64_t;
    pub type __darwin_pid_t = __int32_t;
    pub type __darwin_sigset_t = __uint32_t;
    pub type __darwin_suseconds_t = __int32_t;
    pub type __darwin_uid_t = __uint32_t;
    pub type __darwin_useconds_t = __uint32_t;
    pub type __darwin_uuid_t = [::std::os::raw::c_uchar; 16usize];
    pub type __darwin_uuid_string_t = [::std::os::raw::c_char; 37usize];
    #[repr(C)]
    pub struct __darwin_pthread_handler_rec {
        pub __routine:
            ::std::option::Option<unsafe extern "C" fn(arg1: *mut ::std::os::raw::c_void)>,
        pub __arg: *mut ::std::os::raw::c_void,
        pub __next: *mut __darwin_pthread_handler_rec,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for __darwin_pthread_handler_rec {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    __routine: ref __self_0_0,
                    __arg: ref __self_0_1,
                    __next: ref __self_0_2,
                } => {
                    let debug_trait_builder = &mut ::core::fmt::Formatter::debug_struct(
                        f,
                        "__darwin_pthread_handler_rec",
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__routine",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__arg",
                        &&(*__self_0_1),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__next",
                        &&(*__self_0_2),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for __darwin_pthread_handler_rec {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for __darwin_pthread_handler_rec {
        #[inline]
        fn clone(&self) -> __darwin_pthread_handler_rec {
            {
                let _: ::core::clone::AssertParamIsClone<
                    ::std::option::Option<unsafe extern "C" fn(arg1: *mut ::std::os::raw::c_void)>,
                >;
                let _: ::core::clone::AssertParamIsClone<*mut ::std::os::raw::c_void>;
                let _: ::core::clone::AssertParamIsClone<*mut __darwin_pthread_handler_rec>;
                *self
            }
        }
    }
    #[repr(C)]
    pub struct _opaque_pthread_attr_t {
        pub __sig: ::std::os::raw::c_long,
        pub __opaque: [::std::os::raw::c_char; 56usize],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for _opaque_pthread_attr_t {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    __sig: ref __self_0_0,
                    __opaque: ref __self_0_1,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "_opaque_pthread_attr_t");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__sig",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__opaque",
                        &&(*__self_0_1),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for _opaque_pthread_attr_t {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for _opaque_pthread_attr_t {
        #[inline]
        fn clone(&self) -> _opaque_pthread_attr_t {
            {
                let _: ::core::clone::AssertParamIsClone<::std::os::raw::c_long>;
                let _: ::core::clone::AssertParamIsClone<[::std::os::raw::c_char; 56usize]>;
                *self
            }
        }
    }
    #[repr(C)]
    pub struct _opaque_pthread_cond_t {
        pub __sig: ::std::os::raw::c_long,
        pub __opaque: [::std::os::raw::c_char; 40usize],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for _opaque_pthread_cond_t {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    __sig: ref __self_0_0,
                    __opaque: ref __self_0_1,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "_opaque_pthread_cond_t");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__sig",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__opaque",
                        &&(*__self_0_1),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for _opaque_pthread_cond_t {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for _opaque_pthread_cond_t {
        #[inline]
        fn clone(&self) -> _opaque_pthread_cond_t {
            {
                let _: ::core::clone::AssertParamIsClone<::std::os::raw::c_long>;
                let _: ::core::clone::AssertParamIsClone<[::std::os::raw::c_char; 40usize]>;
                *self
            }
        }
    }
    #[repr(C)]
    pub struct _opaque_pthread_condattr_t {
        pub __sig: ::std::os::raw::c_long,
        pub __opaque: [::std::os::raw::c_char; 8usize],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for _opaque_pthread_condattr_t {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    __sig: ref __self_0_0,
                    __opaque: ref __self_0_1,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "_opaque_pthread_condattr_t");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__sig",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__opaque",
                        &&(*__self_0_1),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for _opaque_pthread_condattr_t {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for _opaque_pthread_condattr_t {
        #[inline]
        fn clone(&self) -> _opaque_pthread_condattr_t {
            {
                let _: ::core::clone::AssertParamIsClone<::std::os::raw::c_long>;
                let _: ::core::clone::AssertParamIsClone<[::std::os::raw::c_char; 8usize]>;
                *self
            }
        }
    }
    #[repr(C)]
    pub struct _opaque_pthread_mutex_t {
        pub __sig: ::std::os::raw::c_long,
        pub __opaque: [::std::os::raw::c_char; 56usize],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for _opaque_pthread_mutex_t {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    __sig: ref __self_0_0,
                    __opaque: ref __self_0_1,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "_opaque_pthread_mutex_t");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__sig",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__opaque",
                        &&(*__self_0_1),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for _opaque_pthread_mutex_t {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for _opaque_pthread_mutex_t {
        #[inline]
        fn clone(&self) -> _opaque_pthread_mutex_t {
            {
                let _: ::core::clone::AssertParamIsClone<::std::os::raw::c_long>;
                let _: ::core::clone::AssertParamIsClone<[::std::os::raw::c_char; 56usize]>;
                *self
            }
        }
    }
    #[repr(C)]
    pub struct _opaque_pthread_mutexattr_t {
        pub __sig: ::std::os::raw::c_long,
        pub __opaque: [::std::os::raw::c_char; 8usize],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for _opaque_pthread_mutexattr_t {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    __sig: ref __self_0_0,
                    __opaque: ref __self_0_1,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "_opaque_pthread_mutexattr_t");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__sig",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__opaque",
                        &&(*__self_0_1),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for _opaque_pthread_mutexattr_t {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for _opaque_pthread_mutexattr_t {
        #[inline]
        fn clone(&self) -> _opaque_pthread_mutexattr_t {
            {
                let _: ::core::clone::AssertParamIsClone<::std::os::raw::c_long>;
                let _: ::core::clone::AssertParamIsClone<[::std::os::raw::c_char; 8usize]>;
                *self
            }
        }
    }
    #[repr(C)]
    pub struct _opaque_pthread_once_t {
        pub __sig: ::std::os::raw::c_long,
        pub __opaque: [::std::os::raw::c_char; 8usize],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for _opaque_pthread_once_t {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    __sig: ref __self_0_0,
                    __opaque: ref __self_0_1,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "_opaque_pthread_once_t");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__sig",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__opaque",
                        &&(*__self_0_1),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for _opaque_pthread_once_t {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for _opaque_pthread_once_t {
        #[inline]
        fn clone(&self) -> _opaque_pthread_once_t {
            {
                let _: ::core::clone::AssertParamIsClone<::std::os::raw::c_long>;
                let _: ::core::clone::AssertParamIsClone<[::std::os::raw::c_char; 8usize]>;
                *self
            }
        }
    }
    #[repr(C)]
    pub struct _opaque_pthread_rwlock_t {
        pub __sig: ::std::os::raw::c_long,
        pub __opaque: [::std::os::raw::c_char; 192usize],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for _opaque_pthread_rwlock_t {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    __sig: ref __self_0_0,
                    __opaque: ref __self_0_1,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "_opaque_pthread_rwlock_t");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__sig",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__opaque",
                        &&(*__self_0_1),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for _opaque_pthread_rwlock_t {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for _opaque_pthread_rwlock_t {
        #[inline]
        fn clone(&self) -> _opaque_pthread_rwlock_t {
            {
                let _: ::core::clone::AssertParamIsClone<::std::os::raw::c_long>;
                let _: ::core::clone::AssertParamIsClone<[::std::os::raw::c_char; 192usize]>;
                *self
            }
        }
    }
    #[repr(C)]
    pub struct _opaque_pthread_rwlockattr_t {
        pub __sig: ::std::os::raw::c_long,
        pub __opaque: [::std::os::raw::c_char; 16usize],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for _opaque_pthread_rwlockattr_t {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    __sig: ref __self_0_0,
                    __opaque: ref __self_0_1,
                } => {
                    let debug_trait_builder = &mut ::core::fmt::Formatter::debug_struct(
                        f,
                        "_opaque_pthread_rwlockattr_t",
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__sig",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__opaque",
                        &&(*__self_0_1),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for _opaque_pthread_rwlockattr_t {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for _opaque_pthread_rwlockattr_t {
        #[inline]
        fn clone(&self) -> _opaque_pthread_rwlockattr_t {
            {
                let _: ::core::clone::AssertParamIsClone<::std::os::raw::c_long>;
                let _: ::core::clone::AssertParamIsClone<[::std::os::raw::c_char; 16usize]>;
                *self
            }
        }
    }
    #[repr(C)]
    pub struct _opaque_pthread_t {
        pub __sig: ::std::os::raw::c_long,
        pub __cleanup_stack: *mut __darwin_pthread_handler_rec,
        pub __opaque: [::std::os::raw::c_char; 8176usize],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for _opaque_pthread_t {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    __sig: ref __self_0_0,
                    __cleanup_stack: ref __self_0_1,
                    __opaque: ref __self_0_2,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "_opaque_pthread_t");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__sig",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__cleanup_stack",
                        &&(*__self_0_1),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "__opaque",
                        &&(*__self_0_2),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for _opaque_pthread_t {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for _opaque_pthread_t {
        #[inline]
        fn clone(&self) -> _opaque_pthread_t {
            {
                let _: ::core::clone::AssertParamIsClone<::std::os::raw::c_long>;
                let _: ::core::clone::AssertParamIsClone<*mut __darwin_pthread_handler_rec>;
                let _: ::core::clone::AssertParamIsClone<[::std::os::raw::c_char; 8176usize]>;
                *self
            }
        }
    }
    pub type __darwin_pthread_attr_t = _opaque_pthread_attr_t;
    pub type __darwin_pthread_cond_t = _opaque_pthread_cond_t;
    pub type __darwin_pthread_condattr_t = _opaque_pthread_condattr_t;
    pub type __darwin_pthread_key_t = ::std::os::raw::c_ulong;
    pub type __darwin_pthread_mutex_t = _opaque_pthread_mutex_t;
    pub type __darwin_pthread_mutexattr_t = _opaque_pthread_mutexattr_t;
    pub type __darwin_pthread_once_t = _opaque_pthread_once_t;
    pub type __darwin_pthread_rwlock_t = _opaque_pthread_rwlock_t;
    pub type __darwin_pthread_rwlockattr_t = _opaque_pthread_rwlockattr_t;
    pub type __darwin_pthread_t = *mut _opaque_pthread_t;
    pub type u_int8_t = ::std::os::raw::c_uchar;
    pub type u_int16_t = ::std::os::raw::c_ushort;
    pub type u_int32_t = ::std::os::raw::c_uint;
    pub type u_int64_t = ::std::os::raw::c_ulonglong;
    pub type register_t = i64;
    pub type user_addr_t = u_int64_t;
    pub type user_size_t = u_int64_t;
    pub type user_ssize_t = i64;
    pub type user_long_t = i64;
    pub type user_ulong_t = u_int64_t;
    pub type user_time_t = i64;
    pub type user_off_t = i64;
    pub type syscall_arg_t = u_int64_t;
    pub type intmax_t = ::std::os::raw::c_long;
    pub type uintmax_t = ::std::os::raw::c_ulong;
    pub const TIME_MACHINE_STRUCT_SIZE: u64 = 1864;
    pub const EXTRA_PUBLISHER_SPACE: u64 = 1000;
    extern "C" {
        pub static sysvar_clock: [u64; 4usize];
    }
    #[repr(C)]
    pub union pc_pub_key {
        pub k1_: [u8; 32usize],
        pub k8_: [u64; 4usize],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for pc_pub_key {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for pc_pub_key {
        #[inline]
        fn clone(&self) -> pc_pub_key {
            {
                let _: ::core::clone::AssertParamIsCopy<Self>;
                *self
            }
        }
    }
    pub type pc_pub_key_t = pc_pub_key;
    #[repr(C)]
    pub struct pc_acc {
        pub magic_: u32,
        pub ver_: u32,
        pub type_: u32,
        pub size_: u32,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for pc_acc {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    magic_: ref __self_0_0,
                    ver_: ref __self_0_1,
                    type_: ref __self_0_2,
                    size_: ref __self_0_3,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "pc_acc");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "magic_",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "ver_",
                        &&(*__self_0_1),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "type_",
                        &&(*__self_0_2),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "size_",
                        &&(*__self_0_3),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for pc_acc {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for pc_acc {
        #[inline]
        fn clone(&self) -> pc_acc {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                *self
            }
        }
    }
    pub type pc_acc_t = pc_acc;
    #[repr(C)]
    pub struct pc_map_table {
        pub magic_: u32,
        pub ver_: u32,
        pub type_: u32,
        pub size_: u32,
        pub num_: u32,
        pub unused_: u32,
        pub next_: pc_pub_key_t,
        pub prod_: [pc_pub_key_t; 640usize],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for pc_map_table {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for pc_map_table {
        #[inline]
        fn clone(&self) -> pc_map_table {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<pc_pub_key_t>;
                let _: ::core::clone::AssertParamIsClone<[pc_pub_key_t; 640usize]>;
                *self
            }
        }
    }
    pub type pc_map_table_t = pc_map_table;
    #[repr(C)]
    pub struct pc_str {
        pub len_: u8,
        pub data_: __IncompleteArrayField<::std::os::raw::c_char>,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for pc_str {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    len_: ref __self_0_0,
                    data_: ref __self_0_1,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "pc_str");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "len_",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "data_",
                        &&(*__self_0_1),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    pub type pc_str_t = pc_str;
    #[repr(C)]
    pub struct pc_prod {
        pub magic_: u32,
        pub ver_: u32,
        pub type_: u32,
        pub size_: u32,
        pub px_acc_: pc_pub_key_t,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for pc_prod {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for pc_prod {
        #[inline]
        fn clone(&self) -> pc_prod {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<pc_pub_key_t>;
                *self
            }
        }
    }
    pub type pc_prod_t = pc_prod;
    #[repr(C)]
    pub struct pc_price_info {
        pub price_: i64,
        pub conf_: u64,
        pub status_: u32,
        pub corp_act_status_: u32,
        pub pub_slot_: u64,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for pc_price_info {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    price_: ref __self_0_0,
                    conf_: ref __self_0_1,
                    status_: ref __self_0_2,
                    corp_act_status_: ref __self_0_3,
                    pub_slot_: ref __self_0_4,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "pc_price_info");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "price_",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "conf_",
                        &&(*__self_0_1),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "status_",
                        &&(*__self_0_2),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "corp_act_status_",
                        &&(*__self_0_3),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "pub_slot_",
                        &&(*__self_0_4),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for pc_price_info {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for pc_price_info {
        #[inline]
        fn clone(&self) -> pc_price_info {
            {
                let _: ::core::clone::AssertParamIsClone<i64>;
                let _: ::core::clone::AssertParamIsClone<u64>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u64>;
                *self
            }
        }
    }
    pub type pc_price_info_t = pc_price_info;
    #[repr(C)]
    pub struct pc_price_comp {
        pub pub_: pc_pub_key_t,
        pub agg_: pc_price_info_t,
        pub latest_: pc_price_info_t,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for pc_price_comp {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for pc_price_comp {
        #[inline]
        fn clone(&self) -> pc_price_comp {
            {
                let _: ::core::clone::AssertParamIsClone<pc_pub_key_t>;
                let _: ::core::clone::AssertParamIsClone<pc_price_info_t>;
                let _: ::core::clone::AssertParamIsClone<pc_price_info_t>;
                *self
            }
        }
    }
    pub type pc_price_comp_t = pc_price_comp;
    #[repr(C)]
    pub struct pc_ema {
        pub val_: i64,
        pub numer_: i64,
        pub denom_: i64,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for pc_ema {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    val_: ref __self_0_0,
                    numer_: ref __self_0_1,
                    denom_: ref __self_0_2,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "pc_ema");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "val_",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "numer_",
                        &&(*__self_0_1),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "denom_",
                        &&(*__self_0_2),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for pc_ema {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for pc_ema {
        #[inline]
        fn clone(&self) -> pc_ema {
            {
                let _: ::core::clone::AssertParamIsClone<i64>;
                let _: ::core::clone::AssertParamIsClone<i64>;
                let _: ::core::clone::AssertParamIsClone<i64>;
                *self
            }
        }
    }
    pub type pc_ema_t = pc_ema;
    #[repr(C)]
    pub struct pc_price {
        pub magic_: u32,
        pub ver_: u32,
        pub type_: u32,
        pub size_: u32,
        pub ptype_: u32,
        pub expo_: i32,
        pub num_: u32,
        pub num_qt_: u32,
        pub last_slot_: u64,
        pub valid_slot_: u64,
        pub twap_: pc_ema_t,
        pub twac_: pc_ema_t,
        pub timestamp_: i64,
        pub min_pub_: u8,
        pub drv2_: i8,
        pub drv3_: i16,
        pub drv4_: i32,
        pub prod_: pc_pub_key_t,
        pub next_: pc_pub_key_t,
        pub prev_slot_: u64,
        pub prev_price_: i64,
        pub prev_conf_: u64,
        pub prev_timestamp_: i64,
        pub agg_: pc_price_info_t,
        pub comp_: [pc_price_comp_t; 32usize],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for pc_price {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for pc_price {
        #[inline]
        fn clone(&self) -> pc_price {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u64>;
                let _: ::core::clone::AssertParamIsClone<u64>;
                let _: ::core::clone::AssertParamIsClone<pc_ema_t>;
                let _: ::core::clone::AssertParamIsClone<pc_ema_t>;
                let _: ::core::clone::AssertParamIsClone<i64>;
                let _: ::core::clone::AssertParamIsClone<u8>;
                let _: ::core::clone::AssertParamIsClone<i8>;
                let _: ::core::clone::AssertParamIsClone<i16>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                let _: ::core::clone::AssertParamIsClone<pc_pub_key_t>;
                let _: ::core::clone::AssertParamIsClone<pc_pub_key_t>;
                let _: ::core::clone::AssertParamIsClone<u64>;
                let _: ::core::clone::AssertParamIsClone<i64>;
                let _: ::core::clone::AssertParamIsClone<u64>;
                let _: ::core::clone::AssertParamIsClone<i64>;
                let _: ::core::clone::AssertParamIsClone<pc_price_info_t>;
                let _: ::core::clone::AssertParamIsClone<[pc_price_comp_t; 32usize]>;
                *self
            }
        }
    }
    pub type pc_price_t = pc_price;
    pub const PRICE_ACCOUNT_SIZE: u64 = 6176;
    pub const command_t_e_cmd_init_mapping: command_t = 0;
    pub const command_t_e_cmd_add_mapping: command_t = 1;
    pub const command_t_e_cmd_add_product: command_t = 2;
    pub const command_t_e_cmd_upd_product: command_t = 3;
    pub const command_t_e_cmd_add_price: command_t = 4;
    pub const command_t_e_cmd_add_publisher: command_t = 5;
    pub const command_t_e_cmd_del_publisher: command_t = 6;
    pub const command_t_e_cmd_upd_price: command_t = 7;
    pub const command_t_e_cmd_agg_price: command_t = 8;
    pub const command_t_e_cmd_init_price: command_t = 9;
    pub const command_t_e_cmd_init_test: command_t = 10;
    pub const command_t_e_cmd_upd_test: command_t = 11;
    pub const command_t_e_cmd_set_min_pub: command_t = 12;
    pub const command_t_e_cmd_upd_price_no_fail_on_error: command_t = 13;
    pub const command_t_e_cmd_resize_price_account: command_t = 14;
    pub const command_t_e_cmd_del_price: command_t = 15;
    pub type command_t = ::std::os::raw::c_uint;
    #[repr(C)]
    pub struct cmd_hdr {
        pub ver_: u32,
        pub cmd_: i32,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for cmd_hdr {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    ver_: ref __self_0_0,
                    cmd_: ref __self_0_1,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "cmd_hdr");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "ver_",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "cmd_",
                        &&(*__self_0_1),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for cmd_hdr {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for cmd_hdr {
        #[inline]
        fn clone(&self) -> cmd_hdr {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                *self
            }
        }
    }
    pub type cmd_hdr_t = cmd_hdr;
    #[repr(C)]
    pub struct cmd_add_product {
        pub ver_: u32,
        pub cmd_: i32,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for cmd_add_product {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    ver_: ref __self_0_0,
                    cmd_: ref __self_0_1,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "cmd_add_product");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "ver_",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "cmd_",
                        &&(*__self_0_1),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for cmd_add_product {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for cmd_add_product {
        #[inline]
        fn clone(&self) -> cmd_add_product {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                *self
            }
        }
    }
    pub type cmd_add_product_t = cmd_add_product;
    #[repr(C)]
    pub struct cmd_upd_product {
        pub ver_: u32,
        pub cmd_: i32,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for cmd_upd_product {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    ver_: ref __self_0_0,
                    cmd_: ref __self_0_1,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "cmd_upd_product");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "ver_",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "cmd_",
                        &&(*__self_0_1),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for cmd_upd_product {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for cmd_upd_product {
        #[inline]
        fn clone(&self) -> cmd_upd_product {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                *self
            }
        }
    }
    pub type cmd_upd_product_t = cmd_upd_product;
    #[repr(C)]
    pub struct cmd_add_price {
        pub ver_: u32,
        pub cmd_: i32,
        pub expo_: i32,
        pub ptype_: u32,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for cmd_add_price {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    ver_: ref __self_0_0,
                    cmd_: ref __self_0_1,
                    expo_: ref __self_0_2,
                    ptype_: ref __self_0_3,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "cmd_add_price");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "ver_",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "cmd_",
                        &&(*__self_0_1),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "expo_",
                        &&(*__self_0_2),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "ptype_",
                        &&(*__self_0_3),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for cmd_add_price {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for cmd_add_price {
        #[inline]
        fn clone(&self) -> cmd_add_price {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                *self
            }
        }
    }
    pub type cmd_add_price_t = cmd_add_price;
    #[repr(C)]
    pub struct cmd_init_price {
        pub ver_: u32,
        pub cmd_: i32,
        pub expo_: i32,
        pub ptype_: u32,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for cmd_init_price {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    ver_: ref __self_0_0,
                    cmd_: ref __self_0_1,
                    expo_: ref __self_0_2,
                    ptype_: ref __self_0_3,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "cmd_init_price");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "ver_",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "cmd_",
                        &&(*__self_0_1),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "expo_",
                        &&(*__self_0_2),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "ptype_",
                        &&(*__self_0_3),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for cmd_init_price {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for cmd_init_price {
        #[inline]
        fn clone(&self) -> cmd_init_price {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                *self
            }
        }
    }
    pub type cmd_init_price_t = cmd_init_price;
    #[repr(C)]
    pub struct cmd_set_min_pub {
        pub ver_: u32,
        pub cmd_: i32,
        pub min_pub_: u8,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for cmd_set_min_pub {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    ver_: ref __self_0_0,
                    cmd_: ref __self_0_1,
                    min_pub_: ref __self_0_2,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "cmd_set_min_pub");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "ver_",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "cmd_",
                        &&(*__self_0_1),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "min_pub_",
                        &&(*__self_0_2),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for cmd_set_min_pub {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for cmd_set_min_pub {
        #[inline]
        fn clone(&self) -> cmd_set_min_pub {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                let _: ::core::clone::AssertParamIsClone<u8>;
                *self
            }
        }
    }
    pub type cmd_set_min_pub_t = cmd_set_min_pub;
    #[repr(C)]
    pub struct cmd_add_publisher {
        pub ver_: u32,
        pub cmd_: i32,
        pub pub_: pc_pub_key_t,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for cmd_add_publisher {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for cmd_add_publisher {
        #[inline]
        fn clone(&self) -> cmd_add_publisher {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                let _: ::core::clone::AssertParamIsClone<pc_pub_key_t>;
                *self
            }
        }
    }
    pub type cmd_add_publisher_t = cmd_add_publisher;
    #[repr(C)]
    pub struct cmd_del_publisher {
        pub ver_: u32,
        pub cmd_: i32,
        pub pub_: pc_pub_key_t,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for cmd_del_publisher {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for cmd_del_publisher {
        #[inline]
        fn clone(&self) -> cmd_del_publisher {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                let _: ::core::clone::AssertParamIsClone<pc_pub_key_t>;
                *self
            }
        }
    }
    pub type cmd_del_publisher_t = cmd_del_publisher;
    #[repr(C)]
    pub struct cmd_upd_price {
        pub ver_: u32,
        pub cmd_: i32,
        pub status_: u32,
        pub unused_: u32,
        pub price_: i64,
        pub conf_: u64,
        pub pub_slot_: u64,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for cmd_upd_price {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    ver_: ref __self_0_0,
                    cmd_: ref __self_0_1,
                    status_: ref __self_0_2,
                    unused_: ref __self_0_3,
                    price_: ref __self_0_4,
                    conf_: ref __self_0_5,
                    pub_slot_: ref __self_0_6,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "cmd_upd_price");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "ver_",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "cmd_",
                        &&(*__self_0_1),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "status_",
                        &&(*__self_0_2),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "unused_",
                        &&(*__self_0_3),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "price_",
                        &&(*__self_0_4),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "conf_",
                        &&(*__self_0_5),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "pub_slot_",
                        &&(*__self_0_6),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for cmd_upd_price {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for cmd_upd_price {
        #[inline]
        fn clone(&self) -> cmd_upd_price {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<i64>;
                let _: ::core::clone::AssertParamIsClone<u64>;
                let _: ::core::clone::AssertParamIsClone<u64>;
                *self
            }
        }
    }
    pub type cmd_upd_price_t = cmd_upd_price;
    #[repr(C)]
    pub struct cmd_upd_test {
        pub ver_: u32,
        pub cmd_: i32,
        pub num_: u32,
        pub expo_: i32,
        pub slot_diff_: [i8; 32usize],
        pub price_: [i64; 32usize],
        pub conf_: [u64; 32usize],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for cmd_upd_test {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    ver_: ref __self_0_0,
                    cmd_: ref __self_0_1,
                    num_: ref __self_0_2,
                    expo_: ref __self_0_3,
                    slot_diff_: ref __self_0_4,
                    price_: ref __self_0_5,
                    conf_: ref __self_0_6,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "cmd_upd_test");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "ver_",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "cmd_",
                        &&(*__self_0_1),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "num_",
                        &&(*__self_0_2),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "expo_",
                        &&(*__self_0_3),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "slot_diff_",
                        &&(*__self_0_4),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "price_",
                        &&(*__self_0_5),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "conf_",
                        &&(*__self_0_6),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for cmd_upd_test {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for cmd_upd_test {
        #[inline]
        fn clone(&self) -> cmd_upd_test {
            {
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                let _: ::core::clone::AssertParamIsClone<u32>;
                let _: ::core::clone::AssertParamIsClone<i32>;
                let _: ::core::clone::AssertParamIsClone<[i8; 32usize]>;
                let _: ::core::clone::AssertParamIsClone<[i64; 32usize]>;
                let _: ::core::clone::AssertParamIsClone<[u64; 32usize]>;
                *self
            }
        }
    }
    pub type cmd_upd_test_t = cmd_upd_test;
    #[repr(C)]
    pub struct sysvar_clock {
        pub slot_: u64,
        pub epoch_start_timestamp_: i64,
        pub epoch_: u64,
        pub leader_schedule_epoch_: u64,
        pub unix_timestamp_: i64,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for sysvar_clock {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    slot_: ref __self_0_0,
                    epoch_start_timestamp_: ref __self_0_1,
                    epoch_: ref __self_0_2,
                    leader_schedule_epoch_: ref __self_0_3,
                    unix_timestamp_: ref __self_0_4,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "sysvar_clock");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "slot_",
                        &&(*__self_0_0),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "epoch_start_timestamp_",
                        &&(*__self_0_1),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "epoch_",
                        &&(*__self_0_2),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "leader_schedule_epoch_",
                        &&(*__self_0_3),
                    );
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "unix_timestamp_",
                        &&(*__self_0_4),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for sysvar_clock {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for sysvar_clock {
        #[inline]
        fn clone(&self) -> sysvar_clock {
            {
                let _: ::core::clone::AssertParamIsClone<u64>;
                let _: ::core::clone::AssertParamIsClone<i64>;
                let _: ::core::clone::AssertParamIsClone<u64>;
                let _: ::core::clone::AssertParamIsClone<u64>;
                let _: ::core::clone::AssertParamIsClone<i64>;
                *self
            }
        }
    }
    pub type sysvar_clock_t = sysvar_clock;
    pub const PC_PRICE_T_COMP_OFFSET: size_t = 240;
    pub const PC_MAP_TABLE_T_PROD_OFFSET: size_t = 56;
    pub type __builtin_va_list = *mut ::std::os::raw::c_char;
    /// The PythAccount trait's purpose is to attach constants to the 3 types of accounts that Pyth has
    /// (mapping, price, product). This allows less duplicated code, because now we can create generic
    /// functions to perform common checks on the accounts and to load and initialize the accounts.
    pub trait PythAccount: Pod {
        /// `ACCOUNT_TYPE` is just the account discriminator, it is different for mapping, product and
        /// price
        const ACCOUNT_TYPE: u32;
        /// `INITIAL_SIZE` is the value that the field `size_` will take when the account is first
        /// initialized this one is slightly tricky because for mapping (resp. price) `size_` won't
        /// include the unpopulated entries of `prod_` (resp. `comp_`). At the beginning there are 0
        /// products (resp. 0 components) therefore `INITIAL_SIZE` will be equal to the offset of
        /// `prod_` (resp. `comp_`)  Similarly the product account `INITIAL_SIZE` won't include any
        /// key values.
        const INITIAL_SIZE: u32;
        /// `minimum_size()` is the minimum size that the solana account holding the struct needs to
        /// have. `INITIAL_SIZE` <= `minimum_size()`
        fn minimum_size() -> usize {
            size_of::<Self>()
        }
    }
    impl PythAccount for pc_map_table_t {
        const ACCOUNT_TYPE: u32 = PC_ACCTYPE_MAPPING;
        const INITIAL_SIZE: u32 = PC_MAP_TABLE_T_PROD_OFFSET as u32;
    }
    impl PythAccount for pc_prod_t {
        const ACCOUNT_TYPE: u32 = PC_ACCTYPE_PRODUCT;
        const INITIAL_SIZE: u32 = size_of::<pc_prod_t>() as u32;
        fn minimum_size() -> usize {
            PC_PROD_ACC_SIZE as usize
        }
    }
    impl PythAccount for pc_price_t {
        const ACCOUNT_TYPE: u32 = PC_ACCTYPE_PRICE;
        const INITIAL_SIZE: u32 = PC_PRICE_T_COMP_OFFSET as u32;
    }
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for pc_acc {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for pc_acc {}
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for pc_map_table {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for pc_map_table {}
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for pc_prod {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for pc_prod {}
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for pc_price {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for pc_price {}
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for cmd_hdr {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for cmd_hdr {}
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for pc_price_info {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for pc_price_info {}
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for cmd_upd_price {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for cmd_upd_price {}
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for pc_ema {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for pc_ema {}
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for cmd_add_price_t {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for cmd_add_price_t {}
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for cmd_init_price_t {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for cmd_init_price_t {}
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for cmd_add_publisher_t {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for cmd_add_publisher_t {}
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for cmd_del_publisher_t {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for cmd_del_publisher_t {}
    unsafe impl Zeroable for cmd_set_min_pub_t {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for cmd_set_min_pub_t {}
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for pc_pub_key_t {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for pc_pub_key_t {}
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for pc_price_comp_t {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for pc_price_comp_t {}
    impl pc_pub_key_t {
        pub fn new_unique() -> pc_pub_key_t {
            let solana_unique = Pubkey::new_unique();
            pc_pub_key_t {
                k1_: solana_unique.to_bytes(),
            }
        }
    }
}
mod deserialize {
    use std::mem::size_of;
    use bytemuck::{try_from_bytes, try_from_bytes_mut, Pod};
    use crate::c_oracle_header::{pc_acc, PythAccount, PC_MAGIC};
    use crate::utils::{clear_account, pyth_assert};
    use solana_program::account_info::AccountInfo;
    use solana_program::program_error::ProgramError;
    use std::cell::{Ref, RefMut};
    /// Interpret the bytes in `data` as a value of type `T`
    pub fn load<T: Pod>(data: &[u8]) -> Result<&T, ProgramError> {
        try_from_bytes(
            data.get(0..size_of::<T>())
                .ok_or(ProgramError::InvalidArgument)?,
        )
        .map_err(|_| ProgramError::InvalidArgument)
    }
    /// Interpret the bytes in `data` as a mutable value of type `T`
    #[allow(unused)]
    pub fn load_mut<T: Pod>(data: &mut [u8]) -> Result<&mut T, ProgramError> {
        try_from_bytes_mut(
            data.get_mut(0..size_of::<T>())
                .ok_or(ProgramError::InvalidArgument)?,
        )
        .map_err(|_| ProgramError::InvalidArgument)
    }
    /// Get the data stored in `account` as a value of type `T`
    pub fn load_account_as<'a, T: Pod>(
        account: &'a AccountInfo,
    ) -> Result<Ref<'a, T>, ProgramError> {
        let data = account.try_borrow_data()?;
        Ok(Ref::map(data, |data| {
            bytemuck::from_bytes(&data[0..size_of::<T>()])
        }))
    }
    /// Mutably borrow the data in `account` as a value of type `T`.
    /// Any mutations to the returned value will be reflected in the account data.
    pub fn load_account_as_mut<'a, T: Pod>(
        account: &'a AccountInfo,
    ) -> Result<RefMut<'a, T>, ProgramError> {
        let data = account.try_borrow_mut_data()?;
        Ok(RefMut::map(data, |data| {
            bytemuck::from_bytes_mut(&mut data[0..size_of::<T>()])
        }))
    }
    pub fn load_checked<'a, T: PythAccount>(
        account: &'a AccountInfo,
        version: u32,
    ) -> Result<RefMut<'a, T>, ProgramError> {
        {
            let account_header = load_account_as::<pc_acc>(account)?;
            pyth_assert(
                account_header.magic_ == PC_MAGIC
                    && account_header.ver_ == version
                    && account_header.type_ == T::ACCOUNT_TYPE,
                ProgramError::InvalidArgument,
            )?;
        }
        load_account_as_mut::<T>(account)
    }
    pub fn initialize_pyth_account_checked<'a, T: PythAccount>(
        account: &'a AccountInfo,
        version: u32,
    ) -> Result<RefMut<'a, T>, ProgramError> {
        clear_account(account)?;
        {
            let mut account_header = load_account_as_mut::<pc_acc>(account)?;
            account_header.magic_ = PC_MAGIC;
            account_header.ver_ = version;
            account_header.type_ = T::ACCOUNT_TYPE;
            account_header.size_ = T::INITIAL_SIZE;
        }
        load_account_as_mut::<T>(account)
    }
}
mod error {
    //! Error types
    use solana_program::program_error::ProgramError;
    use thiserror::Error;
    /// Errors that may be returned by the oracle program
    pub enum OracleError {
        /// Generic catch all error
        #[error("Generic")]
        Generic = 600,
        /// integer casting error
        #[error("IntegerCastingError")]
        IntegerCastingError = 601,
        /// c_entrypoint returned an unexpected value
        #[error("UnknownCError")]
        UnknownCError = 602,
        #[error("UnrecognizedInstruction")]
        UnrecognizedInstruction = 603,
        #[error("InvalidFundingAccount")]
        InvalidFundingAccount = 604,
        #[error("InvalidSignableAccount")]
        InvalidSignableAccount = 605,
        #[error("InvalidSystemAccount")]
        InvalidSystemAccount = 606,
        #[error("InvalidWritableAccount")]
        InvalidWritableAccount = 607,
        #[error("InvalidFreshAccount")]
        InvalidFreshAccount = 608,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for OracleError {
        #[inline]
        fn clone(&self) -> OracleError {
            match (&*self,) {
                (&OracleError::Generic,) => OracleError::Generic,
                (&OracleError::IntegerCastingError,) => OracleError::IntegerCastingError,
                (&OracleError::UnknownCError,) => OracleError::UnknownCError,
                (&OracleError::UnrecognizedInstruction,) => OracleError::UnrecognizedInstruction,
                (&OracleError::InvalidFundingAccount,) => OracleError::InvalidFundingAccount,
                (&OracleError::InvalidSignableAccount,) => OracleError::InvalidSignableAccount,
                (&OracleError::InvalidSystemAccount,) => OracleError::InvalidSystemAccount,
                (&OracleError::InvalidWritableAccount,) => OracleError::InvalidWritableAccount,
                (&OracleError::InvalidFreshAccount,) => OracleError::InvalidFreshAccount,
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for OracleError {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match (&*self,) {
                (&OracleError::Generic,) => ::core::fmt::Formatter::write_str(f, "Generic"),
                (&OracleError::IntegerCastingError,) => {
                    ::core::fmt::Formatter::write_str(f, "IntegerCastingError")
                }
                (&OracleError::UnknownCError,) => {
                    ::core::fmt::Formatter::write_str(f, "UnknownCError")
                }
                (&OracleError::UnrecognizedInstruction,) => {
                    ::core::fmt::Formatter::write_str(f, "UnrecognizedInstruction")
                }
                (&OracleError::InvalidFundingAccount,) => {
                    ::core::fmt::Formatter::write_str(f, "InvalidFundingAccount")
                }
                (&OracleError::InvalidSignableAccount,) => {
                    ::core::fmt::Formatter::write_str(f, "InvalidSignableAccount")
                }
                (&OracleError::InvalidSystemAccount,) => {
                    ::core::fmt::Formatter::write_str(f, "InvalidSystemAccount")
                }
                (&OracleError::InvalidWritableAccount,) => {
                    ::core::fmt::Formatter::write_str(f, "InvalidWritableAccount")
                }
                (&OracleError::InvalidFreshAccount,) => {
                    ::core::fmt::Formatter::write_str(f, "InvalidFreshAccount")
                }
            }
        }
    }
    impl ::core::marker::StructuralEq for OracleError {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::cmp::Eq for OracleError {
        #[inline]
        #[doc(hidden)]
        #[no_coverage]
        fn assert_receiver_is_total_eq(&self) -> () {
            {}
        }
    }
    #[allow(unused_qualifications)]
    impl std::error::Error for OracleError {}
    #[allow(unused_qualifications)]
    impl std::fmt::Display for OracleError {
        fn fmt(&self, __formatter: &mut std::fmt::Formatter) -> std::fmt::Result {
            #[allow(unused_variables, deprecated, clippy::used_underscore_binding)]
            match self {
                OracleError::Generic {} => {
                    let result =
                        __formatter.write_fmt(::core::fmt::Arguments::new_v1(&["Generic"], &[]));
                    result
                }
                OracleError::IntegerCastingError {} => {
                    let result = __formatter.write_fmt(::core::fmt::Arguments::new_v1(
                        &["IntegerCastingError"],
                        &[],
                    ));
                    result
                }
                OracleError::UnknownCError {} => {
                    let result = __formatter
                        .write_fmt(::core::fmt::Arguments::new_v1(&["UnknownCError"], &[]));
                    result
                }
                OracleError::UnrecognizedInstruction {} => {
                    let result = __formatter.write_fmt(::core::fmt::Arguments::new_v1(
                        &["UnrecognizedInstruction"],
                        &[],
                    ));
                    result
                }
                OracleError::InvalidFundingAccount {} => {
                    let result = __formatter.write_fmt(::core::fmt::Arguments::new_v1(
                        &["InvalidFundingAccount"],
                        &[],
                    ));
                    result
                }
                OracleError::InvalidSignableAccount {} => {
                    let result = __formatter.write_fmt(::core::fmt::Arguments::new_v1(
                        &["InvalidSignableAccount"],
                        &[],
                    ));
                    result
                }
                OracleError::InvalidSystemAccount {} => {
                    let result = __formatter.write_fmt(::core::fmt::Arguments::new_v1(
                        &["InvalidSystemAccount"],
                        &[],
                    ));
                    result
                }
                OracleError::InvalidWritableAccount {} => {
                    let result = __formatter.write_fmt(::core::fmt::Arguments::new_v1(
                        &["InvalidWritableAccount"],
                        &[],
                    ));
                    result
                }
                OracleError::InvalidFreshAccount {} => {
                    let result = __formatter.write_fmt(::core::fmt::Arguments::new_v1(
                        &["InvalidFreshAccount"],
                        &[],
                    ));
                    result
                }
            }
        }
    }
    impl ::core::marker::StructuralPartialEq for OracleError {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::cmp::PartialEq for OracleError {
        #[inline]
        fn eq(&self, other: &OracleError) -> bool {
            {
                let __self_vi = ::core::intrinsics::discriminant_value(&*self);
                let __arg_1_vi = ::core::intrinsics::discriminant_value(&*other);
                if true && __self_vi == __arg_1_vi {
                    match (&*self, &*other) {
                        _ => true,
                    }
                } else {
                    false
                }
            }
        }
    }
    impl From<OracleError> for ProgramError {
        fn from(e: OracleError) -> Self {
            ProgramError::Custom(e as u32)
        }
    }
}
mod processor {
    use crate::c_oracle_header::{
        cmd_hdr, command_t_e_cmd_add_mapping, command_t_e_cmd_add_price,
        command_t_e_cmd_add_product, command_t_e_cmd_add_publisher, command_t_e_cmd_agg_price,
        command_t_e_cmd_del_price, command_t_e_cmd_del_publisher, command_t_e_cmd_init_mapping,
        command_t_e_cmd_init_price, command_t_e_cmd_resize_price_account,
        command_t_e_cmd_set_min_pub, command_t_e_cmd_upd_price,
        command_t_e_cmd_upd_price_no_fail_on_error, command_t_e_cmd_upd_product, PC_VERSION,
    };
    use crate::instruction::OracleCommand;
    use crate::deserialize::load;
    use crate::error::OracleError;
    use solana_program::entrypoint::ProgramResult;
    use solana_program::program_error::ProgramError;
    use solana_program::pubkey::Pubkey;
    use solana_program::sysvar::slot_history::AccountInfo;
    use crate::rust_oracle::{
        add_mapping, add_price, add_product, add_publisher, del_price, del_publisher, init_mapping,
        init_price, resize_price_account, set_min_pub, upd_price, upd_price_no_fail_on_error,
        upd_product,
    };
    ///dispatch to the right instruction in the oracle
    pub fn process_instruction(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        instruction_data: &[u8],
    ) -> ProgramResult {
        let cmd_data = load::<cmd_hdr>(instruction_data)?;
        if cmd_data.ver_ != PC_VERSION {
            return Err(ProgramError::InvalidArgument);
        }
        match OracleCommand::try_from(cmd_data.cmd_) {
            OracleCommand::InitMapping => ::core::panicking::panic("not yet implemented"),
            OracleCommand::AddMapping => ::core::panicking::panic("not yet implemented"),
            OracleCommand::AddProduct => ::core::panicking::panic("not yet implemented"),
            OracleCommand::UpdProduct => ::core::panicking::panic("not yet implemented"),
            OracleCommand::AddPrice => ::core::panicking::panic("not yet implemented"),
            OracleCommand::AddPublisher => ::core::panicking::panic("not yet implemented"),
            OracleCommand::DelPublisher => ::core::panicking::panic("not yet implemented"),
            OracleCommand::UpdPrice => ::core::panicking::panic("not yet implemented"),
            OracleCommand::AggPrice => ::core::panicking::panic("not yet implemented"),
            OracleCommand::InitPrice => ::core::panicking::panic("not yet implemented"),
            OracleCommand::InitTest => ::core::panicking::panic("not yet implemented"),
            OracleCommand::UpdTest => ::core::panicking::panic("not yet implemented"),
            OracleCommand::SetMinPub => ::core::panicking::panic("not yet implemented"),
            OracleCommand::UpdPriceNoFailOnError => ::core::panicking::panic("not yet implemented"),
            OracleCommand::ResizePriceAccount => ::core::panicking::panic("not yet implemented"),
            OracleCommand::DelPrice => ::core::panicking::panic("not yet implemented"),
        }
    }
}
mod rust_oracle {
    use std::mem::{size_of, size_of_val};
    use bytemuck::{bytes_of, bytes_of_mut};
    use solana_program::account_info::AccountInfo;
    use solana_program::clock::Clock;
    use solana_program::entrypoint::ProgramResult;
    use solana_program::program::invoke;
    use solana_program::program_error::ProgramError;
    use solana_program::program_memory::{sol_memcpy, sol_memset};
    use solana_program::pubkey::Pubkey;
    use solana_program::rent::Rent;
    use solana_program::system_instruction::transfer;
    use solana_program::system_program::check_id;
    use solana_program::sysvar::Sysvar;
    use crate::c_oracle_header::{
        cmd_add_price_t, cmd_add_publisher_t, cmd_del_publisher_t, cmd_hdr_t, cmd_init_price_t,
        cmd_set_min_pub_t, cmd_upd_price_t, cmd_upd_product_t, pc_ema_t, pc_map_table_t,
        pc_price_comp, pc_price_info_t, pc_price_t, pc_prod_t, pc_pub_key_t, PC_COMP_SIZE,
        PC_MAP_TABLE_SIZE, PC_MAX_CI_DIVISOR, PC_PROD_ACC_SIZE, PC_PTYPE_UNKNOWN,
        PC_STATUS_UNKNOWN, PC_VERSION,
    };
    use crate::deserialize::{initialize_pyth_account_checked, load, load_account_as_mut, load_checked};
    use crate::time_machine_types::PriceAccountWrapper;
    use crate::utils::{
        check_exponent_range, check_valid_fresh_account, check_valid_funding_account,
        check_valid_signable_account, check_valid_writable_account, is_component_update,
        pubkey_assign, pubkey_equal, pubkey_is_zero, pyth_assert, read_pc_str_t, try_convert,
    };
    use crate::OracleError;
    const PRICE_T_SIZE: usize = size_of::<pc_price_t>();
    const PRICE_ACCOUNT_SIZE: usize = size_of::<PriceAccountWrapper>();
    #[cfg(not(target_arch = "bpf"))]
    #[link(name = "cpyth-native")]
    extern "C" {
        pub fn c_upd_aggregate(_input: *mut u8, clock_slot: u64, clock_timestamp: i64) -> bool;
    }
    fn send_lamports<'a>(
        from: &AccountInfo<'a>,
        to: &AccountInfo<'a>,
        system_program: &AccountInfo<'a>,
        amount: u64,
    ) -> Result<(), ProgramError> {
        let transfer_instruction = transfer(from.key, to.key, amount);
        invoke(
            &transfer_instruction,
            &[from.clone(), to.clone(), system_program.clone()],
        )?;
        Ok(())
    }
    /// resizes a price account so that it fits the Time Machine
    /// key[0] funding account       [signer writable]
    /// key[1] price account         [Signer writable]
    /// key[2] system program        [readable]
    pub fn resize_price_account(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        _instruction_data: &[u8],
    ) -> ProgramResult {
        let [funding_account_info, price_account_info, system_program] = match accounts {
            [x, y, z] => Ok([x, y, z]),
            _ => Err(ProgramError::InvalidArgument),
        }?;
        check_valid_funding_account(funding_account_info)?;
        check_valid_signable_account(program_id, price_account_info, size_of::<pc_price_t>())?;
        pyth_assert(
            check_id(system_program.key),
            OracleError::InvalidSystemAccount.into(),
        )?;
        {
            load_checked::<pc_price_t>(price_account_info, PC_VERSION)?;
        }
        let account_len = price_account_info.try_data_len()?;
        match account_len {
            PRICE_T_SIZE => {
                let rent: Rent = Default::default();
                let lamports_needed: u64 = rent
                    .minimum_balance(size_of::<PriceAccountWrapper>())
                    .saturating_sub(price_account_info.lamports());
                if lamports_needed > 0 {
                    send_lamports(
                        funding_account_info,
                        price_account_info,
                        system_program,
                        lamports_needed,
                    )?;
                }
                price_account_info.realloc(size_of::<PriceAccountWrapper>(), false)?;
                let mut price_account =
                    load_checked::<PriceAccountWrapper>(price_account_info, PC_VERSION)?;
                price_account.initialize_time_machine()?;
                Ok(())
            }
            PRICE_ACCOUNT_SIZE => Ok(()),
            _ => Err(ProgramError::InvalidArgument),
        }
    }
    /// initialize the first mapping account in a new linked-list of mapping accounts
    /// accounts[0] funding account           [signer writable]
    /// accounts[1] new mapping account       [signer writable]
    pub fn init_mapping(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        instruction_data: &[u8],
    ) -> ProgramResult {
        let [funding_account, fresh_mapping_account] = match accounts {
            [x, y] => Ok([x, y]),
            _ => Err(ProgramError::InvalidArgument),
        }?;
        check_valid_funding_account(funding_account)?;
        check_valid_signable_account(
            program_id,
            fresh_mapping_account,
            size_of::<pc_map_table_t>(),
        )?;
        check_valid_fresh_account(fresh_mapping_account)?;
        let hdr = load::<cmd_hdr_t>(instruction_data)?;
        initialize_pyth_account_checked::<pc_map_table_t>(fresh_mapping_account, hdr.ver_)?;
        Ok(())
    }
    pub fn add_mapping(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        instruction_data: &[u8],
    ) -> ProgramResult {
        let [funding_account, cur_mapping, next_mapping] = match accounts {
            [x, y, z] => Ok([x, y, z]),
            _ => Err(ProgramError::InvalidArgument),
        }?;
        check_valid_funding_account(funding_account)?;
        check_valid_signable_account(program_id, cur_mapping, size_of::<pc_map_table_t>())?;
        check_valid_signable_account(program_id, next_mapping, size_of::<pc_map_table_t>())?;
        check_valid_fresh_account(next_mapping)?;
        let hdr = load::<cmd_hdr_t>(instruction_data)?;
        let mut cur_mapping = load_checked::<pc_map_table_t>(cur_mapping, hdr.ver_)?;
        pyth_assert(
            cur_mapping.num_ == PC_MAP_TABLE_SIZE && pubkey_is_zero(&cur_mapping.next_),
            ProgramError::InvalidArgument,
        )?;
        initialize_pyth_account_checked::<pc_map_table_t>(next_mapping, hdr.ver_)?;
        pubkey_assign(&mut cur_mapping.next_, &next_mapping.key.to_bytes());
        Ok(())
    }
    /// a publisher updates a price
    /// accounts[0] publisher account                                   [signer writable]
    /// accounts[1] price account to update                             [writable]
    /// accounts[2] sysvar clock                                        []
    pub fn upd_price(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        instruction_data: &[u8],
    ) -> ProgramResult {
        let cmd_args = load::<cmd_upd_price_t>(instruction_data)?;
        let [funding_account, price_account, clock_account] = match accounts {
            [x, y, z] => Ok([x, y, z]),
            [x, y, _, z] => Ok([x, y, z]),
            _ => Err(ProgramError::InvalidArgument),
        }?;
        check_valid_funding_account(funding_account)?;
        check_valid_writable_account(program_id, price_account, size_of::<pc_price_t>())?;
        let clock = Clock::from_account_info(clock_account)?;
        let mut publisher_index: usize = 0;
        let latest_aggregate_price: pc_price_info_t;
        {
            let price_data = load_checked::<pc_price_t>(price_account, cmd_args.ver_)?;
            while publisher_index < price_data.num_ as usize {
                if pubkey_equal(
                    &price_data.comp_[publisher_index].pub_,
                    &funding_account.key.to_bytes(),
                ) {
                    break;
                }
                publisher_index += 1;
            }
            pyth_assert(
                publisher_index < price_data.num_ as usize,
                ProgramError::InvalidArgument,
            )?;
            latest_aggregate_price = price_data.agg_;
            let latest_publisher_price = price_data.comp_[publisher_index].latest_;
            pyth_assert(
                !is_component_update(cmd_args)?
                    || cmd_args.pub_slot_ > latest_publisher_price.pub_slot_,
                ProgramError::InvalidArgument,
            )?;
        }
        let mut aggregate_updated = false;
        if clock.slot > latest_aggregate_price.pub_slot_ {
            unsafe {
                aggregate_updated = c_upd_aggregate(
                    price_account.try_borrow_mut_data()?.as_mut_ptr(),
                    clock.slot,
                    clock.unix_timestamp,
                );
            }
        }
        let account_len = price_account.try_data_len()?;
        if aggregate_updated && account_len == PRICE_ACCOUNT_SIZE {
            let mut price_account = load_account_as_mut::<PriceAccountWrapper>(price_account)?;
            price_account.add_price_to_time_machine()?;
        }
        if is_component_update(cmd_args)? {
            let mut status: u32 = cmd_args.status_;
            let mut threshold_conf = cmd_args.price_ / PC_MAX_CI_DIVISOR as i64;
            if threshold_conf < 0 {
                threshold_conf = -threshold_conf;
            }
            if cmd_args.conf_ > try_convert::<_, u64>(threshold_conf)? {
                status = PC_STATUS_UNKNOWN
            }
            {
                let mut price_data = load_checked::<pc_price_t>(price_account, cmd_args.ver_)?;
                let publisher_price = &mut price_data.comp_[publisher_index].latest_;
                publisher_price.price_ = cmd_args.price_;
                publisher_price.conf_ = cmd_args.conf_;
                publisher_price.status_ = status;
                publisher_price.pub_slot_ = cmd_args.pub_slot_;
            }
        }
        Ok(())
    }
    pub fn upd_price_no_fail_on_error(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        instruction_data: &[u8],
    ) -> ProgramResult {
        match upd_price(program_id, accounts, instruction_data) {
            Err(_) => Ok(()),
            Ok(value) => Ok(value),
        }
    }
    /// add a price account to a product account
    /// accounts[0] funding account                                   [signer writable]
    /// accounts[1] product account to add the price account to       [signer writable]
    /// accounts[2] newly created price account                       [signer writable]
    pub fn add_price(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        instruction_data: &[u8],
    ) -> ProgramResult {
        let cmd_args = load::<cmd_add_price_t>(instruction_data)?;
        check_exponent_range(cmd_args.expo_)?;
        pyth_assert(
            cmd_args.ptype_ != PC_PTYPE_UNKNOWN,
            ProgramError::InvalidArgument,
        )?;
        let [funding_account, product_account, price_account] = match accounts {
            [x, y, z] => Ok([x, y, z]),
            _ => Err(ProgramError::InvalidArgument),
        }?;
        check_valid_funding_account(funding_account)?;
        check_valid_signable_account(program_id, product_account, PC_PROD_ACC_SIZE as usize)?;
        check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;
        check_valid_fresh_account(price_account)?;
        let mut product_data = load_checked::<pc_prod_t>(product_account, cmd_args.ver_)?;
        let mut price_data =
            initialize_pyth_account_checked::<pc_price_t>(price_account, cmd_args.ver_)?;
        price_data.expo_ = cmd_args.expo_;
        price_data.ptype_ = cmd_args.ptype_;
        pubkey_assign(&mut price_data.prod_, &product_account.key.to_bytes());
        pubkey_assign(&mut price_data.next_, bytes_of(&product_data.px_acc_));
        pubkey_assign(&mut product_data.px_acc_, &price_account.key.to_bytes());
        Ok(())
    }
    /// Delete a price account. This function will remove the link between the price account and its
    /// corresponding product account, then transfer any SOL in the price account to the funding
    /// account. This function can only delete the first price account in the linked list of
    /// price accounts for the given product.
    ///
    /// Warning: This function is dangerous and will break any programs that depend on the deleted
    /// price account!
    pub fn del_price(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        instruction_data: &[u8],
    ) -> ProgramResult {
        let [funding_account, product_account, price_account] = match accounts {
            [w, x, y] => Ok([w, x, y]),
            _ => Err(ProgramError::InvalidArgument),
        }?;
        check_valid_funding_account(funding_account)?;
        check_valid_signable_account(program_id, product_account, PC_PROD_ACC_SIZE as usize)?;
        check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;
        {
            let cmd_args = load::<cmd_hdr_t>(instruction_data)?;
            let mut product_data = load_checked::<pc_prod_t>(product_account, cmd_args.ver_)?;
            let price_data = load_checked::<pc_price_t>(price_account, cmd_args.ver_)?;
            pyth_assert(
                pubkey_equal(&product_data.px_acc_, &price_account.key.to_bytes()),
                ProgramError::InvalidArgument,
            )?;
            pyth_assert(
                pubkey_equal(&price_data.prod_, &product_account.key.to_bytes()),
                ProgramError::InvalidArgument,
            )?;
            pubkey_assign(&mut product_data.px_acc_, bytes_of(&price_data.next_));
        }
        let lamports = price_account.lamports();
        **price_account.lamports.borrow_mut() = 0;
        **funding_account.lamports.borrow_mut() += lamports;
        Ok(())
    }
    pub fn init_price(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        instruction_data: &[u8],
    ) -> ProgramResult {
        let cmd_args = load::<cmd_init_price_t>(instruction_data)?;
        check_exponent_range(cmd_args.expo_)?;
        let [funding_account, price_account] = match accounts {
            [x, y] => Ok([x, y]),
            _ => Err(ProgramError::InvalidArgument),
        }?;
        check_valid_funding_account(funding_account)?;
        check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;
        let mut price_data = load_checked::<pc_price_t>(price_account, cmd_args.ver_)?;
        pyth_assert(
            price_data.ptype_ == cmd_args.ptype_,
            ProgramError::InvalidArgument,
        )?;
        price_data.expo_ = cmd_args.expo_;
        price_data.last_slot_ = 0;
        price_data.valid_slot_ = 0;
        price_data.agg_.pub_slot_ = 0;
        price_data.prev_slot_ = 0;
        price_data.prev_price_ = 0;
        price_data.prev_conf_ = 0;
        price_data.prev_timestamp_ = 0;
        sol_memset(
            bytes_of_mut(&mut price_data.twap_),
            0,
            size_of::<pc_ema_t>(),
        );
        sol_memset(
            bytes_of_mut(&mut price_data.twac_),
            0,
            size_of::<pc_ema_t>(),
        );
        sol_memset(
            bytes_of_mut(&mut price_data.agg_),
            0,
            size_of::<pc_price_info_t>(),
        );
        for i in 0..(price_data.comp_.len() as usize) {
            sol_memset(
                bytes_of_mut(&mut price_data.comp_[i].agg_),
                0,
                size_of::<pc_price_info_t>(),
            );
            sol_memset(
                bytes_of_mut(&mut price_data.comp_[i].latest_),
                0,
                size_of::<pc_price_info_t>(),
            );
        }
        Ok(())
    }
    /// add a publisher to a price account
    /// accounts[0] funding account                                   [signer writable]
    /// accounts[1] price account to add the publisher to             [signer writable]
    pub fn add_publisher(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        instruction_data: &[u8],
    ) -> ProgramResult {
        let cmd_args = load::<cmd_add_publisher_t>(instruction_data)?;
        pyth_assert(
            instruction_data.len() == size_of::<cmd_add_publisher_t>()
                && !pubkey_is_zero(&cmd_args.pub_),
            ProgramError::InvalidArgument,
        )?;
        let [funding_account, price_account] = match accounts {
            [x, y] => Ok([x, y]),
            _ => Err(ProgramError::InvalidArgument),
        }?;
        check_valid_funding_account(funding_account)?;
        check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;
        let mut price_data = load_checked::<pc_price_t>(price_account, cmd_args.ver_)?;
        if price_data.num_ >= PC_COMP_SIZE {
            return Err(ProgramError::InvalidArgument);
        }
        for i in 0..(price_data.num_ as usize) {
            if pubkey_equal(&cmd_args.pub_, bytes_of(&price_data.comp_[i].pub_)) {
                return Err(ProgramError::InvalidArgument);
            }
        }
        let current_index: usize = try_convert(price_data.num_)?;
        sol_memset(
            bytes_of_mut(&mut price_data.comp_[current_index]),
            0,
            size_of::<pc_price_comp>(),
        );
        pubkey_assign(
            &mut price_data.comp_[current_index].pub_,
            bytes_of(&cmd_args.pub_),
        );
        price_data.num_ += 1;
        price_data.size_ =
            try_convert::<_, u32>(size_of::<pc_price_t>() - size_of_val(&price_data.comp_))?
                + price_data.num_ * try_convert::<_, u32>(size_of::<pc_price_comp>())?;
        Ok(())
    }
    /// add a publisher to a price account
    /// accounts[0] funding account                                   [signer writable]
    /// accounts[1] price account to delete the publisher from        [signer writable]
    pub fn del_publisher(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        instruction_data: &[u8],
    ) -> ProgramResult {
        let cmd_args = load::<cmd_del_publisher_t>(instruction_data)?;
        pyth_assert(
            instruction_data.len() == size_of::<cmd_del_publisher_t>()
                && !pubkey_is_zero(&cmd_args.pub_),
            ProgramError::InvalidArgument,
        )?;
        let [funding_account, price_account] = match accounts {
            [x, y] => Ok([x, y]),
            _ => Err(ProgramError::InvalidArgument),
        }?;
        check_valid_funding_account(funding_account)?;
        check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;
        let mut price_data = load_checked::<pc_price_t>(price_account, cmd_args.ver_)?;
        for i in 0..(price_data.num_ as usize) {
            if pubkey_equal(&cmd_args.pub_, bytes_of(&price_data.comp_[i].pub_)) {
                for j in i + 1..(price_data.num_ as usize) {
                    price_data.comp_[j - 1] = price_data.comp_[j];
                }
                price_data.num_ -= 1;
                let current_index: usize = try_convert(price_data.num_)?;
                sol_memset(
                    bytes_of_mut(&mut price_data.comp_[current_index]),
                    0,
                    size_of::<pc_price_comp>(),
                );
                price_data.size_ = try_convert::<_, u32>(
                    size_of::<pc_price_t>() - size_of_val(&price_data.comp_),
                )? + price_data.num_
                    * try_convert::<_, u32>(size_of::<pc_price_comp>())?;
                return Ok(());
            }
        }
        Err(ProgramError::InvalidArgument)
    }
    pub fn add_product(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        instruction_data: &[u8],
    ) -> ProgramResult {
        let [funding_account, tail_mapping_account, new_product_account] = match accounts {
            [x, y, z] => Ok([x, y, z]),
            _ => Err(ProgramError::InvalidArgument),
        }?;
        check_valid_funding_account(funding_account)?;
        check_valid_signable_account(
            program_id,
            tail_mapping_account,
            size_of::<pc_map_table_t>(),
        )?;
        check_valid_signable_account(program_id, new_product_account, PC_PROD_ACC_SIZE as usize)?;
        check_valid_fresh_account(new_product_account)?;
        let hdr = load::<cmd_hdr_t>(instruction_data)?;
        let mut mapping_data = load_checked::<pc_map_table_t>(tail_mapping_account, hdr.ver_)?;
        pyth_assert(
            mapping_data.num_ < PC_MAP_TABLE_SIZE,
            ProgramError::InvalidArgument,
        )?;
        initialize_pyth_account_checked::<pc_prod_t>(new_product_account, hdr.ver_)?;
        let current_index: usize = try_convert(mapping_data.num_)?;
        pubkey_assign(
            &mut mapping_data.prod_[current_index],
            bytes_of(&new_product_account.key.to_bytes()),
        );
        mapping_data.num_ += 1;
        mapping_data.size_ =
            try_convert::<_, u32>(size_of::<pc_map_table_t>() - size_of_val(&mapping_data.prod_))?
                + mapping_data.num_ * try_convert::<_, u32>(size_of::<pc_pub_key_t>())?;
        Ok(())
    }
    /// Update the metadata associated with a product, overwriting any existing metadata.
    /// The metadata is provided as a list of key-value pairs at the end of the `instruction_data`.
    pub fn upd_product(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        instruction_data: &[u8],
    ) -> ProgramResult {
        let [funding_account, product_account] = match accounts {
            [x, y] => Ok([x, y]),
            _ => Err(ProgramError::InvalidArgument),
        }?;
        check_valid_funding_account(funding_account)?;
        check_valid_signable_account(program_id, product_account, try_convert(PC_PROD_ACC_SIZE)?)?;
        let hdr = load::<cmd_hdr_t>(instruction_data)?;
        {
            let mut _product_data = load_checked::<pc_prod_t>(product_account, hdr.ver_)?;
        }
        pyth_assert(
            instruction_data.len() >= size_of::<cmd_upd_product_t>(),
            ProgramError::InvalidInstructionData,
        )?;
        let new_data_len = instruction_data.len() - size_of::<cmd_upd_product_t>();
        let max_data_len = try_convert::<_, usize>(PC_PROD_ACC_SIZE)? - size_of::<pc_prod_t>();
        pyth_assert(new_data_len <= max_data_len, ProgramError::InvalidArgument)?;
        let new_data = &instruction_data[size_of::<cmd_upd_product_t>()..instruction_data.len()];
        let mut idx = 0;
        while idx < new_data.len() {
            let key = read_pc_str_t(&new_data[idx..])?;
            idx += key.len();
            let value = read_pc_str_t(&new_data[idx..])?;
            idx += value.len();
        }
        pyth_assert(idx == new_data.len(), ProgramError::InvalidArgument)?;
        {
            let mut data = product_account.try_borrow_mut_data()?;
            sol_memcpy(
                &mut data[size_of::<pc_prod_t>()..],
                new_data,
                new_data.len(),
            );
        }
        let mut product_data = load_checked::<pc_prod_t>(product_account, hdr.ver_)?;
        product_data.size_ = try_convert(size_of::<pc_prod_t>() + new_data.len())?;
        Ok(())
    }
    pub fn set_min_pub(
        program_id: &Pubkey,
        accounts: &[AccountInfo],
        instruction_data: &[u8],
    ) -> ProgramResult {
        let cmd = load::<cmd_set_min_pub_t>(instruction_data)?;
        pyth_assert(
            instruction_data.len() == size_of::<cmd_set_min_pub_t>(),
            ProgramError::InvalidArgument,
        )?;
        let [funding_account, price_account] = match accounts {
            [x, y] => Ok([x, y]),
            _ => Err(ProgramError::InvalidArgument),
        }?;
        check_valid_funding_account(funding_account)?;
        check_valid_signable_account(program_id, price_account, size_of::<pc_price_t>())?;
        let mut price_account_data = load_checked::<pc_price_t>(price_account, cmd.ver_)?;
        price_account_data.min_pub_ = cmd.min_pub_;
        Ok(())
    }
}
mod time_machine_types {
    use crate::c_oracle_header::{
        pc_price_t, PythAccount, EXTRA_PUBLISHER_SPACE, PC_ACCTYPE_PRICE, PC_PRICE_T_COMP_OFFSET,
    };
    use crate::error::OracleError;
    use bytemuck::{Pod, Zeroable};
    use solana_program::msg;
    #[repr(C)]
    /// this wraps multiple SMA and tick trackers, and includes all the state
    /// used by the time machine
    pub struct TimeMachineWrapper {
        place_holder: [u8; 1864],
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::fmt::Debug for TimeMachineWrapper {
        fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
            match *self {
                Self {
                    place_holder: ref __self_0_0,
                } => {
                    let debug_trait_builder =
                        &mut ::core::fmt::Formatter::debug_struct(f, "TimeMachineWrapper");
                    let _ = ::core::fmt::DebugStruct::field(
                        debug_trait_builder,
                        "place_holder",
                        &&(*__self_0_0),
                    );
                    ::core::fmt::DebugStruct::finish(debug_trait_builder)
                }
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for TimeMachineWrapper {
        #[inline]
        fn clone(&self) -> TimeMachineWrapper {
            {
                let _: ::core::clone::AssertParamIsClone<[u8; 1864]>;
                *self
            }
        }
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for TimeMachineWrapper {}
    #[repr(C)]
    /// wraps everything stored in a price account
    pub struct PriceAccountWrapper {
        pub price_data: pc_price_t,
        pub extra_publisher_space: [u8; EXTRA_PUBLISHER_SPACE as usize],
        pub time_machine: TimeMachineWrapper,
    }
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::marker::Copy for PriceAccountWrapper {}
    #[automatically_derived]
    #[allow(unused_qualifications)]
    impl ::core::clone::Clone for PriceAccountWrapper {
        #[inline]
        fn clone(&self) -> PriceAccountWrapper {
            {
                let _: ::core::clone::AssertParamIsClone<pc_price_t>;
                let _: ::core::clone::AssertParamIsClone<[u8; EXTRA_PUBLISHER_SPACE as usize]>;
                let _: ::core::clone::AssertParamIsClone<TimeMachineWrapper>;
                *self
            }
        }
    }
    impl PriceAccountWrapper {
        pub fn initialize_time_machine(&mut self) -> Result<(), OracleError> {
            ::solana_program::log::sol_log("implement me");
            Ok(())
        }
        pub fn add_price_to_time_machine(&mut self) -> Result<(), OracleError> {
            ::solana_program::log::sol_log("implement me");
            Ok(())
        }
    }
    #[cfg(target_endian = "little")]
    unsafe impl Zeroable for PriceAccountWrapper {}
    #[cfg(target_endian = "little")]
    unsafe impl Pod for PriceAccountWrapper {}
    impl PythAccount for PriceAccountWrapper {
        const ACCOUNT_TYPE: u32 = PC_ACCTYPE_PRICE;
        const INITIAL_SIZE: u32 = PC_PRICE_T_COMP_OFFSET as u32;
    }
}
mod instruction {
    use num_derive::FromPrimitive;
    #[repr(i32)]
    pub enum OracleCommand {
        InitMapping,
        AddMapping,
        AddProduct,
        UpdProduct,
        AddPrice,
        AddPublisher,
        DelPublisher,
        UpdPrice,
        AggPrice,
        InitPrice,
        InitTest,
        UpdTest,
        SetMinPub,
        UpdPriceNoFailOnError,
        ResizePriceAccount,
        DelPrice,
    }
    #[allow(non_upper_case_globals, unused_qualifications)]
    const _IMPL_NUM_FromPrimitive_FOR_OracleCommand: () = {
        #[allow(clippy::useless_attribute)]
        #[allow(rust_2018_idioms)]
        extern crate num_traits as _num_traits;
        impl _num_traits::FromPrimitive for OracleCommand {
            #[allow(trivial_numeric_casts)]
            #[inline]
            fn from_i64(n: i64) -> Option<Self> {
                if n == OracleCommand::InitMapping as i64 {
                    Some(OracleCommand::InitMapping)
                } else if n == OracleCommand::AddMapping as i64 {
                    Some(OracleCommand::AddMapping)
                } else if n == OracleCommand::AddProduct as i64 {
                    Some(OracleCommand::AddProduct)
                } else if n == OracleCommand::UpdProduct as i64 {
                    Some(OracleCommand::UpdProduct)
                } else if n == OracleCommand::AddPrice as i64 {
                    Some(OracleCommand::AddPrice)
                } else if n == OracleCommand::AddPublisher as i64 {
                    Some(OracleCommand::AddPublisher)
                } else if n == OracleCommand::DelPublisher as i64 {
                    Some(OracleCommand::DelPublisher)
                } else if n == OracleCommand::UpdPrice as i64 {
                    Some(OracleCommand::UpdPrice)
                } else if n == OracleCommand::AggPrice as i64 {
                    Some(OracleCommand::AggPrice)
                } else if n == OracleCommand::InitPrice as i64 {
                    Some(OracleCommand::InitPrice)
                } else if n == OracleCommand::InitTest as i64 {
                    Some(OracleCommand::InitTest)
                } else if n == OracleCommand::UpdTest as i64 {
                    Some(OracleCommand::UpdTest)
                } else if n == OracleCommand::SetMinPub as i64 {
                    Some(OracleCommand::SetMinPub)
                } else if n == OracleCommand::UpdPriceNoFailOnError as i64 {
                    Some(OracleCommand::UpdPriceNoFailOnError)
                } else if n == OracleCommand::ResizePriceAccount as i64 {
                    Some(OracleCommand::ResizePriceAccount)
                } else if n == OracleCommand::DelPrice as i64 {
                    Some(OracleCommand::DelPrice)
                } else {
                    None
                }
            }
            #[inline]
            fn from_u64(n: u64) -> Option<Self> {
                Self::from_i64(n as i64)
            }
        }
    };
}
mod utils {
    use crate::c_oracle_header::{
        cmd_upd_price_t, command_t_e_cmd_upd_price, command_t_e_cmd_upd_price_no_fail_on_error,
        pc_acc, pc_pub_key_t, PC_MAX_NUM_DECIMALS,
    };
    use crate::deserialize::load_account_as;
    use crate::OracleError;
    use solana_program::account_info::AccountInfo;
    use solana_program::program_error::ProgramError;
    use solana_program::program_memory::sol_memset;
    use solana_program::pubkey::Pubkey;
    use solana_program::sysvar::rent::Rent;
    use std::borrow::BorrowMut;
    pub fn pyth_assert(condition: bool, error_code: ProgramError) -> Result<(), ProgramError> {
        if !condition {
            Result::Err(error_code)
        } else {
            Result::Ok(())
        }
    }
    /// Sets the data of account to all-zero
    pub fn clear_account(account: &AccountInfo) -> Result<(), ProgramError> {
        let mut data = account
            .try_borrow_mut_data()
            .map_err(|_| ProgramError::InvalidArgument)?;
        let length = data.len();
        sol_memset(data.borrow_mut(), 0, length);
        Ok(())
    }
    pub fn valid_funding_account(account: &AccountInfo) -> bool {
        account.is_signer && account.is_writable
    }
    pub fn check_valid_funding_account(account: &AccountInfo) -> Result<(), ProgramError> {
        pyth_assert(
            valid_funding_account(account),
            OracleError::InvalidFundingAccount.into(),
        )
    }
    pub fn valid_signable_account(
        program_id: &Pubkey,
        account: &AccountInfo,
        minimum_size: usize,
    ) -> bool {
        account.is_signer
            && account.is_writable
            && account.owner == program_id
            && account.data_len() >= minimum_size
            && Rent::default().is_exempt(account.lamports(), account.data_len())
    }
    pub fn check_valid_signable_account(
        program_id: &Pubkey,
        account: &AccountInfo,
        minimum_size: usize,
    ) -> Result<(), ProgramError> {
        pyth_assert(
            valid_signable_account(program_id, account, minimum_size),
            OracleError::InvalidSignableAccount.into(),
        )
    }
    /// Returns `true` if the `account` is fresh, i.e., its data can be overwritten.
    /// Use this check to prevent accidentally overwriting accounts whose data is already populated.
    pub fn valid_fresh_account(account: &AccountInfo) -> bool {
        let pyth_acc = load_account_as::<pc_acc>(account);
        match pyth_acc {
            Ok(pyth_acc) => pyth_acc.magic_ == 0 && pyth_acc.ver_ == 0,
            Err(_) => false,
        }
    }
    pub fn check_valid_fresh_account(account: &AccountInfo) -> Result<(), ProgramError> {
        pyth_assert(
            valid_fresh_account(account),
            OracleError::InvalidFreshAccount.into(),
        )
    }
    pub fn check_exponent_range(expo: i32) -> Result<(), ProgramError> {
        pyth_assert(
            expo >= -(PC_MAX_NUM_DECIMALS as i32) && expo <= PC_MAX_NUM_DECIMALS as i32,
            ProgramError::InvalidArgument,
        )
    }
    pub fn pubkey_assign(target: &mut pc_pub_key_t, source: &[u8]) {
        unsafe { target.k1_.copy_from_slice(source) }
    }
    pub fn pubkey_is_zero(key: &pc_pub_key_t) -> bool {
        return unsafe { key.k8_.iter().all(|x| *x == 0) };
    }
    pub fn pubkey_equal(target: &pc_pub_key_t, source: &[u8]) -> bool {
        unsafe { target.k1_ == *source }
    }
    /// Convert `x: T` into a `U`, returning the appropriate `OracleError` if the conversion fails.
    pub fn try_convert<T, U: TryFrom<T>>(x: T) -> Result<U, OracleError> {
        U::try_from(x).map_err(|_| OracleError::IntegerCastingError)
    }
    /// Read a `pc_str_t` from the beginning of `source`. Returns a slice of `source` containing
    /// the bytes of the `pc_str_t`.
    pub fn read_pc_str_t(source: &[u8]) -> Result<&[u8], ProgramError> {
        if source.is_empty() {
            Err(ProgramError::InvalidArgument)
        } else {
            let tag_len: usize = try_convert(source[0])?;
            if tag_len + 1 > source.len() {
                Err(ProgramError::InvalidArgument)
            } else {
                Ok(&source[..(1 + tag_len)])
            }
        }
    }
    fn valid_writable_account(
        program_id: &Pubkey,
        account: &AccountInfo,
        minimum_size: usize,
    ) -> bool {
        account.is_writable
            && account.owner == program_id
            && account.data_len() >= minimum_size
            && Rent::default().is_exempt(account.lamports(), account.data_len())
    }
    pub fn check_valid_writable_account(
        program_id: &Pubkey,
        account: &AccountInfo,
        minimum_size: usize,
    ) -> Result<(), ProgramError> {
        pyth_assert(
            valid_writable_account(program_id, account, minimum_size),
            OracleError::InvalidWritableAccount.into(),
        )
    }
    /// Checks whether this instruction is trying to update an individual publisher's price (`true`) or
    /// is only trying to refresh the aggregate (`false`)
    pub fn is_component_update(cmd_args: &cmd_upd_price_t) -> Result<bool, ProgramError> {
        Ok(
            try_convert::<_, u32>(cmd_args.cmd_)? == command_t_e_cmd_upd_price
                || try_convert::<_, u32>(cmd_args.cmd_)?
                    == command_t_e_cmd_upd_price_no_fail_on_error,
        )
    }
}
use crate::error::OracleError;
use processor::process_instruction;
use solana_program::entrypoint;
/// # Safety
#[no_mangle]
pub unsafe extern "C" fn entrypoint(input: *mut u8) -> u64 {
    let (program_id, accounts, instruction_data) =
        unsafe { ::solana_program::entrypoint::deserialize(input) };
    match process_instruction(&program_id, &accounts, &instruction_data) {
        Ok(()) => ::solana_program::entrypoint::SUCCESS,
        Err(error) => error.into(),
    }
}
