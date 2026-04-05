#!/usr/bin/env python3
import sys
import re
from collections import defaultdict

def parse_trace(filename):
    pairs = []
    with open(filename) as f:
        for line in f:
            m = re.search(r'Address:\s*(\d+).*Status:\s*([01])', line)
            if m:
                pairs.append((int(m.group(1)), int(m.group(2))))
    return pairs

def check_interleaved(pairs, while_addr, if_addr):
    n = len(pairs)
    for start in range(n):
        if pairs[start][0] != while_addr or pairs[start][1] != 1:
            continue
        i            = start
        if_count_run = 0
        while i < n:
            if pairs[i][0] != while_addr:
                break
            if pairs[i][1] == 0:
                break
            if i + 1 >= n or pairs[i+1][0] != if_addr:
                break
            if_count_run += 1
            i += 2
        if if_count_run > 0 and i < n and pairs[i][0] == while_addr and pairs[i][1] == 0:
            return True, start
    return False, -1

def main():
    filename = sys.argv[1] if len(sys.argv) > 1 else "key.out"
    pairs    = parse_trace(filename)

    count = defaultdict(int)
    seqs  = defaultdict(list)
    for addr, taken in pairs:
        count[addr] += 1
        seqs[addr].append(taken)

    # WHILE: count=64, sequence is 111...10
    while_addr = None
    for addr, seq in seqs.items():
        if count[addr] != 64:
            continue
        if seq[-1] == 0 and all(b == 1 for b in seq[:-1]):
            while_addr = addr
            break

    if while_addr is None:
        print("[!] WHILE address not found.")
        return

    # IF: count=64, not while, interleaved with while
    if_candidates = [a for a, c in count.items() if c == 64 and a != while_addr]

    for if_addr in if_candidates:
        interleaved, start_idx = check_interleaved(pairs, while_addr, if_addr)
        if not interleaved:
            continue

        # Collect IF bits from interleaved region
        key_bits = []
        i = start_idx
        while i < len(pairs):
            if pairs[i][0] != while_addr:
                break
            if pairs[i][1] == 0:
                break
            i += 1
            key_bits.append(pairs[i][1])
            i += 1

        # Negate bits: taken=1 means key bit=0, taken=0 means key bit=1
        key = 0
        for b in key_bits:
            key = (key << 1) | (b ^ 1)

        with open("key.txt", "w") as f:
            f.write(f"{key}\n")

        print(key)
        return

    print("[!] IF address not found.")

if __name__ == "__main__":
    main()
