// This file links the various test_*.c files in the C oracle code so they run via cargo.
// Note that most of these tests are exhaustive (testing almost every possible input/output), so
// they take a minute or so to run.
mod c {
    #[link(name = "cpyth-test")]
    extern "C" {
        pub fn test_price_model() -> i32;
        pub fn test_sort_stable() -> i32;
        pub fn test_align() -> i32;
        pub fn test_avg() -> i32;
        pub fn test_hash() -> i32;
        pub fn test_prng() -> i32;
        pub fn test_round() -> i32;
        pub fn test_sar() -> i32;
    }
}

#[test]
fn test_price_model() {
    unsafe {
        assert_eq!(c::test_price_model(), 0);
    }
}

#[test]
fn test_sort_stable() {
    unsafe {
        assert_eq!(c::test_sort_stable(), 0);
    }
}

#[test]
fn test_align() {
    unsafe {
        assert_eq!(c::test_align(), 0);
    }
}

#[test]
fn test_avg() {
    unsafe {
        assert_eq!(c::test_avg(), 0);
    }
}

#[test]
fn test_hash() {
    unsafe {
        assert_eq!(c::test_hash(), 0);
    }
}

#[test]
fn test_prng() {
    unsafe {
        assert_eq!(c::test_prng(), 0);
    }
}

#[test]
fn test_round() {
    unsafe {
        assert_eq!(c::test_round(), 0);
    }
}

#[test]
fn test_sar() {
    unsafe {
        assert_eq!(c::test_sar(), 0);
    }
}
