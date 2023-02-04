use crate::List::*;
use NextElem as Next;

enum List {
    NextElem(u32, Box<List>),
    End,
}

impl List {
    fn new() -> List {
        End
    }

    fn prepend(self, elem: u32) -> List {
        Next(elem, Box::new(self))
    }

    fn prepend_list(self, elems: &[u32]) -> List {
        let mut temp = self;
        for elem in elems {
            temp = temp.prepend(*elem);
        }
        temp
    }

    fn len(&self) -> u32 {
        match *self {
            Next(_, ref next) => 1 + next.len(),
            End => 0
        }
    }

    fn stringify(&self) -> String {
        format!("Liniked List: ");
        match *self {
            Next(val, ref next) => {
                format!("{}, {}", val, next.stringify())
            }
            End => {
                format!("End of Linked List")
            }
        }
    }
}

fn main() {
    let mut list = List::new();

    list = list.prepend(1).prepend(2).prepend(3);

    list = list.prepend_list(&[1,2,3]);

    println!("list length is {}", list.len());
    println!("{}", list.stringify());
}