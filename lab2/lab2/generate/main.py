import random
import string

def generate_random_string(length):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

num_lines = 25000

with open('input.dat', 'w') as file:
    for _ in range(num_lines):
        line = generate_random_string(256)
        file.write(line + '\n')

print(f"Файл 'input.dat' создан с {num_lines} строками по 40 символов.")
