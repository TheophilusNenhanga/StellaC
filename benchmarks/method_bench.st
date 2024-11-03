class Zoo {
    fn init() {
        self.aardvark = 1;
        self.baboon = 1;
        self.cat = 1;
        self.donkey = 1;
        self.elephant = 1;
        self.fox = 1;
        self.giraffe = 1;
        self.hippo = 1;
        self.iguana = 1;
        self.jaguar = 1;
        self.kangaroo = 1;
        self.lion = 1;
    }

    fn ant() { return self.aardvark; }
    fn banana() { return self.baboon; }
    fn tuna() { return self.cat; }
    fn hay() { return self.donkey; }
    fn grass() { return self.elephant; }
    fn mouse() { return self.fox; }
    fn leaves() { return self.giraffe; }
    fn water() { return self.hippo; }
    fn insects() { return self.iguana; }
    fn meat() { return self.jaguar; }
    fn hop() { return self.kangaroo; }
    fn roar() { return self.lion; }
}

let zoo = Zoo();
let sum = 0;
let start = time_ms();
while sum < 100000000 {
    sum = sum + zoo.ant()
                + zoo.banana()
                + zoo.tuna()
                + zoo.hay()
                + zoo.grass()
                + zoo.mouse()
                + zoo.leaves()
                + zoo.water()
                + zoo.insects()
                + zoo.meat()
                + zoo.hop()
                + zoo.roar();
}
print(time_ms() - start);
print(sum);
