x = 0

while x < 10000:
    if x % 3 == 0 and x % 5 == 0:
        print("fizzbuzz");
    elif x % 3 == 0:
        print("fizz")
    elif x % 5 == 0:
        print("buzz")
    print(x);
    x += 1
