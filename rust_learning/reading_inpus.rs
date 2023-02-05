use std::io;
use std::thread;
use std::sync::mpsc;
use std::process;

fn main() {
    println!("This program takes your response in one thread and prints a response in another");

    let (tx, rx) = mpsc::channel();
    thread::spawn(move || {
        let mut answer = String::from("");
        while answer != String::from("Quit") {
            answer = String::from("");
            io::stdin().read_line(&mut answer).unwrap();
            tx.send(answer.clone()).unwrap();
        }
    });

    println!("Now listening");

    for message in rx.iter() {
        let matcher = message.split("\r").next();

        println!("{}", match matcher {
            Some(a) if a == "Hello" => "Nice to meet you".to_string(),
            Some(a) if a == "Quit" => {
                process::exit(1);
                // "Goodbye".to_string()
            },
            Some(a) if a.contains("What") => "I don't answer questions".to_string(),
            Some(a) => format!("What does {} mean?", a),
            _ => "Try again".to_string(),
        });
    }
}