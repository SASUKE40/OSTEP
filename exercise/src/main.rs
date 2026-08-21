fn main() {
    println!("Hello, world!");
    let x = 5;
    println!("The value of x is: {x}");
    println!("The type of x is: {}", std::any::type_name_of_val(&x));
    println!(
        "The interproduct of 2, 3, and 4 is: {}",
        interproduct(2, 3, 4)
    );

    let y = 30;
    take_u32(x);
    take_i8(y);
    let mut point = (1, 2);
    let x_coord = &mut point.0;
    *x_coord += 1;
    println!("The point is now: {:?}", point);

    let mut x_ref = &x;
    println!("The value of x_ref is: {}", x_ref);
    x_ref = &12;
    println!("The value of x_ref is now: {}", x_ref);
    let y_ref: &mut i32 = &mut 12;
    println!("The value of y_ref is: {:p}", y_ref);
    let player_move = PlayerMove::Move(Direction::Up);
    println!("The player move is: {:?}", player_move);
}

#[derive(Debug)]
enum Direction {
    Up,
    Down,
    Left,
    Right,
}

#[derive(Debug)]
enum PlayerMove {
    Move(Direction),
    Stop,
}

fn interproduct(a: i32, b: i32, c: i32) -> i32 {
    a * b + b * c + a * c
}

fn take_u32(x: u32) {
    println!("u32: {x}");
}

fn take_i8(x: i8) {
    println!("i8: {x}");
}
