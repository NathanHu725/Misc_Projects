use std::process::{Command, Stdio};
use std::io::prelude::*;

static CASE: &'static str = "Hello\r\n";

fn main() {
    let process = match Command::new("reading_inputs.exe")
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .spawn() {
            Err(why) => panic!("Couldn't make because {}", why),
            Ok(process) => process,
        };

    println!("Made Process");

    match process.stdin.unwrap().write_all(CASE.as_bytes()) {
        Err(why) => panic!("Couldn't make because {}", why),
        Ok(_) => print!("sent"),
    }

    let mut s = String::new();
    match process.stdout.unwrap().read_to_string(&mut s) {
        Err(why) => panic!("couldn't read dir stdout: {}", why),
        Ok(_) => print!("wc responded with:\n{}", s),
    }
    
    // let output = Command::new("threads.exe").output().unwrap();

    // if output.status.success() {
    //     let s = String::from_utf8_lossy(&output.stdout);

    //     print!("rustc succeeded and stdout was:\n{}", s);
    // } else {
    //     println!("Failure");
    // }
}