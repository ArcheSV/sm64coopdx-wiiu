#!/usr/bin/env python3
import argparse
import struct


ENDIAN = {
    "big": ">",
    "little": "<",
}

def align(value, amount):
    return (value + amount - 1) & -amount

def word_bytes(bitwidth):
    return int(bitwidth) // 8

def header_size(word_size):
    return 4 if word_size == 4 else 8

def read_u16(data, pos, endian):
    return struct.unpack_from(ENDIAN[endian] + "H", data, pos)[0]

def read_u32(data, pos, endian):
    return struct.unpack_from(ENDIAN[endian] + "I", data, pos)[0]

def read_i32(data, pos, endian):
    return struct.unpack_from(ENDIAN[endian] + "i", data, pos)[0]

def read_f32(data, pos, endian):
    return struct.unpack_from(ENDIAN[endian] + "f", data, pos)[0]

def read_ptr(data, pos, endian, size):
    fmt = "I" if size == 4 else "Q"
    return struct.unpack_from(ENDIAN[endian] + fmt, data, pos)[0]

def pack_u16(value, endian):
    return struct.pack(ENDIAN[endian] + "H", value)

def pack_u32(value, endian):
    return struct.pack(ENDIAN[endian] + "I", value)

def pack_i32(value, endian):
    return struct.pack(ENDIAN[endian] + "i", value)

def pack_f32(value, endian):
    return struct.pack(ENDIAN[endian] + "f", value)

def pack_ptr(value, endian, size):
    fmt = "I" if size == 4 else "Q"
    return struct.pack(ENDIAN[endian] + fmt, value)

# non-shindou seqfile layout: HHX header, then pointer/offset + length entries
def read_seqfile_entries(data, args):
    src_word = word_bytes(args.source_bitwidth)
    magic = read_u16(data, 0, args.source_endian)
    count = read_u16(data, 2, args.source_endian)
    if magic not in (1, 2, 3):
        raise ValueError(f"{args.input}: unexpected seqfile magic {magic}")

    table_pos = header_size(src_word)
    entry_size = src_word * 2
    table_end = table_pos + count * entry_size
    if len(data) < table_end:
        raise ValueError(f"{args.input}: truncated seqfile table")

    entries = []
    for index in range(count):
        pos = table_pos + index * entry_size
        offset = read_ptr(data, pos, args.source_endian, src_word)
        length = read_u32(data, pos + src_word, args.source_endian)
        entries.append((offset, length))
    return magic, count, entries


# ctl/tbl entries must be inside the blob, but coop dx sequences can be fake
def validate_seqfile_ranges(data, entries, strict, label):
    for index, (offset, length) in enumerate(entries):
        if offset == 0 and length == 0:
            continue
        if offset + length > len(data):
            message = (
                f"{label}: entry {index} range outside file: "
                f"offset=0x{offset:x} length=0x{length:x} file=0x{len(data):x}"
            )
            if strict:
                raise ValueError(message)

# keep payload in-place and only rewrite the outer seqfile table
def write_seqfile_table(data, magic, entries, args):
    dst_word = word_bytes(args.target_bitwidth)
    required = header_size(dst_word) + len(entries) * dst_word * 2
    if len(data) < required:
        raise ValueError(f"{args.input}: output seqfile table would exceed file")

    data[0:required] = b"\0" * required
    data[0:2] = pack_u16(magic, args.target_endian)
    data[2:4] = pack_u16(len(entries), args.target_endian)

    table_pos = header_size(dst_word)
    for index, (offset, length) in enumerate(entries):
        pos = table_pos + index * dst_word * 2
        data[pos:pos + dst_word] = pack_ptr(offset, args.target_endian, dst_word)
        data[pos + dst_word:pos + dst_word + 4] = pack_u32(length, args.target_endian)

def convert_seqfile(args):
    with open(args.input, "rb") as f:
        data = bytearray(f.read())

    magic, _, entries = read_seqfile_entries(data, args)
    validate_seqfile_ranges(data, entries, magic in (1, 2), args.input)
    write_seqfile_table(data, magic, entries, args)

    with open(args.output, "wb") as f:
        f.write(data)

def read_seq_count(path, endian, bitwidth):
    with open(path, "rb") as f:
        header = f.read(header_size(word_bytes(bitwidth)))
    if len(header) < 4:
        raise ValueError(f"{path}: sequence file is too small")

    magic = read_u16(header, 0, endian)
    count = read_u16(header, 2, endian)
    if magic not in (1, 2, 3):
        raise ValueError(f"{path}: unexpected seqfile magic {magic}")
    return count

class Writer:
    def __init__(self):
        self.data = bytearray()

    @property
    def pos(self):
        return len(self.data)
    def add(self, payload):
        self.data += payload
    def reserve(self, size):
        pos = len(self.data)
        self.data += b"\0" * size
        return pos
    def patch(self, pos, payload):
        self.data[pos:pos + len(payload)] = payload
    def align(self, amount):
        self.data += b"\0" * (align(len(self.data), amount) - len(self.data))

# semantically repack ctl banks instead of byte-swapping the whole blob
class CtlBankRepacker:
    def __init__(self, source, args, num_instruments, num_drums):
        self.source = source
        self.args = args
        self.src_word = word_bytes(args.source_bitwidth)
        self.dst_word = word_bytes(args.target_bitwidth)
        self.num_instruments = num_instruments
        self.num_drums = num_drums
        self.writer = Writer()
        self.samples = {}
        self.books = {}
        self.loops = {}
        self.envelopes = {}
        self.instruments = {}
        self.drums = {}
        self.covered = []

    def src_ptr(self, pos):
        return read_ptr(self.source, pos, self.args.source_endian, self.src_word)

    def check(self, pos, size, label):
        if pos < 0 or pos + size > len(self.source):
            raise ValueError(f"{label}: source offset out of range 0x{pos:x}")
        self.covered.append((pos, pos + size, label))

    # fail if the parser leaves non-zero source data behind
    def assert_no_uncovered_nonzero(self):
        covered = bytearray(len(self.source))
        for start, end, _ in self.covered:
            covered[start:end] = b"\x01" * (end - start)

        self.mark_unreferenced_envelopes(covered)

        for index, value in enumerate(self.source):
            if value != 0 and not covered[index]:
                raise ValueError(
                    f"ctl bank has uncovered non-zero byte at 0x{index:x}: 0x{value:02x}"
                )

    # assemble_sound.py can leave unused envelope blobs in the bank
    def mark_unreferenced_envelopes(self, covered):
        for start in range(0, len(self.source), 4):
            if start + 4 > len(self.source) or covered[start]:
                continue

            cursor = start
            entries = 0
            end = None
            while cursor + 4 <= len(self.source):
                delay = struct.unpack_from(">H", self.source, cursor)[0]
                cursor += 4
                entries += 1
                if delay in (0x0000, 0xFFFF, 0xFFFE, 0xFFFD):
                    end = cursor
                    break
                if entries > 1024:
                    break

            if end is None or entries < 2:
                continue

            aligned_end = min(align(end, 16), len(self.source))
            if any(self.source[index] != 0 and not covered[index] for index in range(start, aligned_end)):
                covered[start:aligned_end] = b"\x01" * (aligned_end - start)

    def ptr_field_offset(self):
        return 4 if self.src_word == 4 else 8

    def sound_size(self):
        return self.src_word * 2

    # rebuild the bank block, internal pointers are relative to this block
    def repack(self):
        top_size = self.dst_word * (1 + self.num_instruments)
        self.writer.reserve(top_size)
        self.writer.align(16)

        self.check(0, self.src_word * (1 + self.num_instruments), "audio bank")
        src_drums = self.src_ptr(0)
        src_instruments = [
            self.src_ptr(self.src_word * (index + 1))
            for index in range(self.num_instruments)
        ]

        dst_drums = self.write_drum_table(src_drums) if src_drums and self.num_drums else 0
        self.writer.patch(0, pack_ptr(dst_drums, self.args.target_endian, self.dst_word))

        for index, src_inst in enumerate(src_instruments):
            dst_inst = self.write_instrument(src_inst) if src_inst else 0
            self.writer.patch(
                self.dst_word * (index + 1),
                pack_ptr(dst_inst, self.args.target_endian, self.dst_word),
            )

        self.writer.align(16)
        self.assert_no_uncovered_nonzero()
        return bytes(self.writer.data)

    def write_drum_table(self, src_offset):
        self.check(src_offset, self.num_drums * self.src_word, "drum table")
        dst_drums = []
        for index in range(self.num_drums):
            src_drum = self.src_ptr(src_offset + index * self.src_word)
            dst_drums.append(self.write_drum(src_drum) if src_drum else 0)

        pos = self.writer.pos
        for dst_drum in dst_drums:
            self.writer.add(pack_ptr(dst_drum, self.args.target_endian, self.dst_word))
        self.writer.align(16)
        return pos

    def write_instrument(self, src_offset):
        if src_offset in self.instruments:
            return self.instruments[src_offset]

        ptr_pos = self.ptr_field_offset()
        sound_base = ptr_pos + self.src_word
        self.check(src_offset, sound_base + self.sound_size() * 3, "instrument")

        dst_env = self.write_envelope(self.src_ptr(src_offset + ptr_pos))
        low = self.make_sound(src_offset + sound_base)
        normal = self.make_sound(src_offset + sound_base + self.sound_size())
        high = self.make_sound(src_offset + sound_base + self.sound_size() * 2)

        pos = self.writer.pos
        self.instruments[src_offset] = pos
        self.writer.add(self.source[src_offset:src_offset + 4])
        if self.dst_word == 8:
            self.writer.add(b"\0" * 4)
        self.writer.add(pack_ptr(dst_env, self.args.target_endian, self.dst_word))
        self.writer.add(low)
        self.writer.add(normal)
        self.writer.add(high)
        self.writer.align(16)
        return pos

    def write_drum(self, src_offset):
        if src_offset in self.drums:
            return self.drums[src_offset]

        sound_pos = self.ptr_field_offset()
        env_pos = sound_pos + self.sound_size()
        self.check(src_offset, env_pos + self.src_word, "drum")

        sound = self.make_sound(src_offset + sound_pos)
        dst_env = self.write_envelope(self.src_ptr(src_offset + env_pos))

        pos = self.writer.pos
        self.drums[src_offset] = pos
        self.writer.add(self.source[src_offset:src_offset + 4])
        if self.dst_word == 8:
            self.writer.add(b"\0" * 4)
        self.writer.add(sound)
        self.writer.add(pack_ptr(dst_env, self.args.target_endian, self.dst_word))
        self.writer.align(16)
        return pos

    def make_sound(self, src_pos):
        self.check(src_pos, self.sound_size(), "sound")
        src_sample = self.src_ptr(src_pos)
        dst_sample = self.write_sample(src_sample) if src_sample else 0
        tuning = read_f32(self.source, src_pos + self.src_word, self.args.source_endian)

        payload = bytearray()
        payload += pack_ptr(dst_sample, self.args.target_endian, self.dst_word)
        payload += pack_f32(tuning, self.args.target_endian)
        if self.dst_word == 8:
            payload += b"\0" * 4
        return bytes(payload)

    # sample object: metadata, tbl offset, loop pointer, book pointer, size
    def write_sample(self, src_offset):
        if src_offset in self.samples:
            return self.samples[src_offset]

        ptr_pos = self.ptr_field_offset()
        sample_size_pos = ptr_pos + self.src_word * 3
        self.check(src_offset, sample_size_pos + 4, "sample")

        sample_header = read_u32(self.source, src_offset, self.args.source_endian)
        sample_addr = self.src_ptr(src_offset + ptr_pos)
        loop = self.write_loop(self.src_ptr(src_offset + ptr_pos + self.src_word))
        book = self.write_book(self.src_ptr(src_offset + ptr_pos + self.src_word * 2))
        sample_size = read_u32(self.source, src_offset + sample_size_pos, self.args.source_endian)

        pos = self.writer.pos
        self.samples[src_offset] = pos
        self.writer.add(pack_u32(sample_header, self.args.target_endian))
        if self.dst_word == 8:
            self.writer.add(b"\0" * 4)
        self.writer.add(pack_ptr(sample_addr, self.args.target_endian, self.dst_word))
        self.writer.add(pack_ptr(loop, self.args.target_endian, self.dst_word))
        self.writer.add(pack_ptr(book, self.args.target_endian, self.dst_word))
        self.writer.add(pack_u32(sample_size, self.args.target_endian))
        self.writer.align(16)
        return pos

    # adpcm loop uses pack("IIiI"), so count is signed
    def write_loop(self, src_offset):
        if src_offset == 0:
            return 0
        if src_offset in self.loops:
            return self.loops[src_offset]

        self.check(src_offset, 16, "adpcm loop")
        start = read_u32(self.source, src_offset + 0, self.args.source_endian)
        end = read_u32(self.source, src_offset + 4, self.args.source_endian)
        count = read_i32(self.source, src_offset + 8, self.args.source_endian)
        pad = read_u32(self.source, src_offset + 12, self.args.source_endian)

        pos = self.writer.pos
        self.loops[src_offset] = pos
        self.writer.add(pack_u32(start, self.args.target_endian))
        self.writer.add(pack_u32(end, self.args.target_endian))
        self.writer.add(pack_i32(count, self.args.target_endian))
        self.writer.add(pack_u32(pad, self.args.target_endian))

        if count != 0:
            self.check(src_offset + 16, 32, "adpcm loop state")
            for index in range(16):
                value = read_u16(self.source, src_offset + 16 + index * 2, self.args.source_endian)
                self.writer.add(pack_u16(value, self.args.target_endian))
        self.writer.align(16)
        return pos

    def write_book(self, src_offset):
        if src_offset == 0:
            return 0
        if src_offset in self.books:
            return self.books[src_offset]

        self.check(src_offset, 8, "adpcm book")
        order = read_i32(self.source, src_offset + 0, self.args.source_endian)
        npredictors = read_i32(self.source, src_offset + 4, self.args.source_endian)
        table_count = 8 * order * npredictors
        if table_count < 0:
            raise ValueError("adpcm book has invalid dimensions")
        self.check(src_offset + 8, table_count * 2, "adpcm book table")

        pos = self.writer.pos
        self.books[src_offset] = pos
        self.writer.add(pack_i32(order, self.args.target_endian))
        self.writer.add(pack_i32(npredictors, self.args.target_endian))
        for index in range(table_count):
            value = read_u16(self.source, src_offset + 8 + index * 2, self.args.source_endian)
            self.writer.add(pack_u16(value, self.args.target_endian))
        self.writer.align(16)
        return pos

    # adsr envelopes are always stored big-endian
    def write_envelope(self, src_offset):
        if src_offset == 0:
            return 0
        if src_offset in self.envelopes:
            return self.envelopes[src_offset]

        pos = self.writer.pos
        self.envelopes[src_offset] = pos

        cursor = src_offset
        while True:
            self.check(cursor, 4, "adsr envelope")
            delay = struct.unpack_from(">H", self.source, cursor)[0]
            self.writer.add(self.source[cursor:cursor + 4])
            cursor += 4
            if delay in (0x0000, 0xFFFF, 0xFFFE, 0xFFFD):
                break
        self.writer.align(16)
        return pos


# convert the outer ctl seqfile and then repack each bank entry
def convert_ctl(args):
    with open(args.input, "rb") as f:
        data = bytearray(f.read())

    magic, _, entries = read_seqfile_entries(data, args)
    validate_seqfile_ranges(data, entries, True, args.input)
    write_seqfile_table(data, magic, entries, args)

    for offset, length in entries:
        if length == 0:
            continue
        if offset + length > len(data) or length < 16:
            raise ValueError(f"ctl entry at 0x{offset:x} is invalid")

        entry = data[offset:offset + length]
        header = bytearray()
        num_instruments = read_u32(entry, 0, args.source_endian)
        num_drums = read_u32(entry, 4, args.source_endian)
        header += pack_u32(num_instruments, args.target_endian)
        header += pack_u32(num_drums, args.target_endian)
        header += pack_u32(read_u32(entry, 8, args.source_endian), args.target_endian)
        header += pack_u32(read_u32(entry, 12, args.source_endian), args.target_endian)

        bank = CtlBankRepacker(entry[16:], args, num_instruments, num_drums).repack()
        converted = header + bank
        if len(converted) > length:
            raise ValueError(f"converted ctl entry grew past original size at 0x{offset:x}")

        # make conversion errors fail here instead of later in cemu
        validation_args = argparse.Namespace(
            source_endian=args.target_endian,
            target_endian=args.target_endian,
            source_bitwidth=args.target_bitwidth,
            target_bitwidth=args.target_bitwidth,
        )
        CtlBankRepacker(
            converted[16:],
            validation_args,
            num_instruments,
            num_drums,
        ).repack()

        converted += b"\0" * (length - len(converted))
        data[offset:offset + length] = converted

    with open(args.output, "wb") as f:
        f.write(data)


def convert_bank_sets(args):
    count = read_seq_count(args.count_from_seqfile, args.seqfile_endian, args.target_bitwidth)
    with open(args.input, "rb") as f:
        data = bytearray(f.read())

    table_end = count * 2
    if len(data) < table_end:
        raise ValueError(f"{args.input}: truncated bank set table")

    for index in range(count):
        pos = index * 2
        value = read_u16(data, pos, args.source_endian)
        data[pos:pos + 2] = pack_u16(value, args.target_endian)

    with open(args.output, "wb") as f:
        f.write(data)


def main():
    parser = argparse.ArgumentParser(description="Convert SM64 sound data endian/bitwidth.")
    parser.add_argument("mode", choices=("seqfile", "ctl", "banksets"))
    parser.add_argument("input")
    parser.add_argument("output")
    parser.add_argument("--from", dest="source_endian", choices=ENDIAN, required=True)
    parser.add_argument("--to", dest="target_endian", choices=ENDIAN, required=True)
    parser.add_argument("--from-bitwidth", dest="source_bitwidth", choices=("32", "64"), default="64")
    parser.add_argument("--to-bitwidth", dest="target_bitwidth", choices=("32", "64"), default="32")
    parser.add_argument("--count-from-seqfile")
    parser.add_argument("--seqfile-endian", choices=ENDIAN, default="big")
    args = parser.parse_args()

    if args.mode == "seqfile":
        convert_seqfile(args)
    elif args.mode == "ctl":
        convert_ctl(args)
    else:
        if not args.count_from_seqfile:
            parser.error("banksets mode requires --count-from-seqfile")
        convert_bank_sets(args)


if __name__ == "__main__":
    main()
