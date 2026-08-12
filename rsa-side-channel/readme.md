# RSA Key Extractor

---

## Files

| File | Description |
|---|---|
| `tracer.cpp` | PIN tool source code |
| `makefile` | Build file |
| `analyse.py` | Python script to extract key from trace |
| `README.md` | This file |

---

## Requirements

- Intel PIN 4.1 (`pin-4.1-99687-d9b8f822c`)
- Python 3
- 64-bit Linux

---

## Compilation
```bash
cd pin-4.1-99687-d9b8f822c/source/tools/rsa-side-channel/
make obj-intel64/tracer.so
```

---

## Usage

### Step 1: Run PIN tool to generate trace
```bash
./pin (path to pin executable) -t ./source/tools/rsa-side-channel/obj-intel64/tracer.so (path to shared object) -o key.out -- ./source/tools/rsa-side-channel/leaky_rsa (path to leaky_rsa binary)
```

This generates `key.out` containing the branch trace.

### Step 2: Run Python script to extract key
```bash
python3 analyse.py key.out
```

This generates `key.txt` containing the extracted key in decimal.

### Step 3: Verify
```bash
./source/tools/rsa-side-channel/leaky_rsa -a $(cat key.txt)
# Expected output: Your Key is Correct
```

---

## How It Works

The binary uses the **square-and-multiply** algorithm for RSA decryption which is vulnerable to side channel attacks:
```
result = 1
for i = LSB to MSB:
    result = square(result)
    if key_bit[i] == 0:
        result = multiply(result, M)
return result
```

The PIN tool records every conditional branch inside the main executable. The trace is then analyzed by `analyse.py`:

1. **WHILE branch** — address with count=64 whose sequence is `111...10` (all taken except the final loop exit)
2. **IF branch** — the other address with count=64 that is strictly interleaved with the WHILE branch in the raw trace
3. **Key bits** — extracted from IF branch taken values and negated (branch is taken when key bit=0, not taken when key bit=1)
4. Key is reconstructed and written as a single decimal value to `key.txt`

---

## Output

`key.txt` contains a single line with the extracted key in decimal:
```
2753
```
