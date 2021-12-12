use pyth_bindgen::Generator;

fn main() {
  let generator = Generator::default();
  let header = generator.src_dir.join("oracle/oracle.h");
  generator.bind_header(&header).unwrap();
}
