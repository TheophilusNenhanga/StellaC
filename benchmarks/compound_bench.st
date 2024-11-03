
fn testCompoundOperator(n) {
    let sum = 0;
    let start = time_ms();
    for let i = 0; i < n; i = i + 1 {
        sum += 1;
        sum *= 4;
        sum /= 2;
        sum -= 2;
    }
    let end = time_ms();
    return end - start;
}

fn testBasicAssignment(n) {
    let sum = 0;
    let start = time_ms();
    for let i = 0; i < n; i = i + 1 {
        sum = sum + 1;
        sum = sum * 4;
        sum = sum / 2;
        sum = sum - 2;
    }
    let end = time_ms();
    return end - start;
}

let iterations = 100000000;
let compoundTime = testCompoundOperator(iterations);
let assignmentTime = testBasicAssignment(iterations);

println("Compound operator time:");
println(compoundTime);
println("Basic assignment time:");
println(assignmentTime);

if compoundTime < assignmentTime {
    println("Compound operators are faster.");
} else if assignmentTime < compoundTime {
    println("Basic assignment is faster.");
} else {
    println("Both methods took the same time.");
}
