use std::sync::mpsc;
use std::thread;
use std::time::{Instant, Duration};

const NTHREADS: i32 = 5;

fn main() {
    let (tx, rx) = mpsc::channel();
    let mut children = Vec::new();
    let start_time = Instant::now();

    for id in 0..NTHREADS {
        let tx = tx.clone();
        let st = start_time.clone();
        children.push(thread::spawn(move || {
            let curr_time = st.elapsed().as_nanos();
            tx.send(curr_time).unwrap();

            println!("thread {} sent {}", id, curr_time);
        }))
    }

    let mut ids = Vec::with_capacity(NTHREADS as usize);
    for _ in 0..NTHREADS {
        ids.push(rx.recv());
    }

    for child in children {
        child.join().expect("child thread panicked");
    }

    println!("All threads finished running, the outputs are {:?}", ids);
}