use std::thread;
const num_of_threads: i32 = 10;

fn main() {
    let data = "86967897737416471853297327050364959
11861322575564723963297542624962850
70856234701860851907960690014725639
38397966707106094172783238747669219
52380795257888236525459303330302837
58495327135744041048897885734297812
69920216438980873548808413720956532
16278424637452589860345374828574668".split_whitespace().collect::<String>();

    let chunked_data = data.chars().enumerate().fold(String::from("\n"), |acc, (i, c)| {
        if i != 0 && i % (data.chars().count() / num_of_threads as usize) == 0 {
            format!("{} {}", acc, c)
        } else {
            format!("{}{}", acc, c)
        }
    });
    
    let result: u32 = thread::scope(|s| {
        let mut children = vec![];        
        for chunk in chunked_data.split_whitespace() {
            children.push(s.spawn(move || -> u32 {
                chunk.chars().map(|c| c.to_digit(10).expect("not a digit")).sum()
            }));
        }
        children.into_iter().map(|c| c.join().unwrap()).sum::<u32>()
    });
    println!("Final sum is {}", result);
}