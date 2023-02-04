use std::fmt;

#[derive(Debug)]
struct Matrix(f32, f32, f32, f32);

impl fmt::Display for Matrix {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let first_row = (self.0, self.1);
        let second_row = (self.2, self.3);

        write!(f, "{:?}\n", first_row)?;
        write!(f, "{:?}", second_row)
    } 
}

fn transpose(orig: Matrix) -> Matrix {
    Matrix(orig.0, orig.2, orig.1, orig.3)
}

struct Point {
    x: f32,
    y: f32,
}

struct Rectangle {
    top_left: Point,
    bottom_right: Point,
}

impl Rectangle {
    fn area(&self) -> f64 {
        let Rectangle{
            top_left: Point{x:x1, y:y1}, 
            bottom_right: Point{x:x2, y:y2}
        } = self;

        ((x1 - x2) * (y1 - y2)).abs().into()
    }
}

enum Work {
    Civilian,
    Soldier,
    Potato,
}

fn main() {
    println!("1 + 2 {}", 1u32 + 2);

    println!("1 - 2 {}", 1i32 - 2);

        // Tuples can be tuple members
    let tuple_of_tuples = ((1u8, 2u16, 2u32), (4u64, -1i8), -2i16);

    // Tuples are printable
    println!("tuple of tuples: {:?}", tuple_of_tuples);

    let matrix = Matrix(1.1, 1.2, 2.1, 2.2);
    println!("{}", matrix);
    
    println!("Matrix:\n{}", matrix);
    println!("Transpose:\n{}", transpose(matrix)); 

    let point: Point = Point { x: 10.3, y: 0.4 };

    println!("point coordinates: ({}, {})", point.x, point.y);

    let rect: Rectangle = Rectangle { top_left: point, bottom_right: Point { x: 10.5, y: 0.0}};
    println!("The area of the rectangle is {}", rect.area());

    use crate::Work::*;
    let work = Civilian;
    match work {
        Civilian => println!("Not a fighter"),
        Soldier  => println!("Is a fitghter"),
        _ => println!("Potato"),
    }
}