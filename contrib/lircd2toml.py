#!/usr/bin/python3
#
# Copyright (C) 2018 Sean Young <sean@mess.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 2 of the License.
#

import sys
import os
import math
import argparse

class LircdParser:
    def __init__(self, filename, encoding):
        self.lineno=0
        self.filename=filename
        self.encoding=encoding

    def getline(self):
        while True:
            self.lineno += 1
            line = ""
            try:
                line = self.fh.readline();
            except IOError as e:
                self.error("reading file failed: {}".format(e))
            except UnicodeDecodeError as e:
                self.error("decoding file failed: {}".format(e))
            if line == "":
                return None
            line = line.strip()
            if len(line) == 0 or line[0] == '#':
                continue
            return line

    def warning(self, msg):
        print("{}:remote {}: warning: {}".format(self.filename, self.lineno, msg), file=sys.stderr)

    def error(self, msg):
        print("{}:remote {}: error: {}".format(self.filename, self.lineno, msg), file=sys.stderr)

    def parse(self):
        remotes = []
        try:
            self.fh=open(self.filename, encoding=self.encoding)
        except IOError as e:
            print("{}: error: {}".format(self.filename, e), file=sys.stderr)
            return remotes

        while True:
            line = self.getline()
            if line == None:
                break
            a = line.split(maxsplit=2)
            if a[0] != 'begin' or a[1] != 'remote':
                self.error("expected 'begin remote', got '{} {}'".format(a[0], a[1]))
                return None
            if len(a) > 2 and a[2][0] != '#':
                self.error("unexpected {}".format(a[2]))
                return None

            remote = self.read_remote()
            if remote == None:
                return None

            remotes.append(remote)

        return remotes

    def read_remote(self):
        remote = {}
        while True:
            line = self.getline()
            if line == None:
                self.error("unexpected end of file")
                return None

            a = line.split()
            if len(a) < 2:
                self.error("expecting at least two keywords")
                return None

            if a[0] in ['name', 'driver', 'serial_mode']:
                remote[a[0]] = line.split(maxsplit=2)[1]
            elif a[0] in ['flags']:
                flags = []
                s=line.split(maxsplit=2)[1]
                for f in s.split(sep='|'):
                    flags.append(f.strip().lower())
                remote[ 'flags' ] = flags
            elif a[0] == 'begin':
                if a[1] == 'codes':
                    codes = self.read_codes()
                    if codes == None:
                        return None
                    remote['codes'] = codes
                elif a[1] == 'raw_codes':
                    codes = self.read_raw_codes()
                    if codes == None:
                        return None
                    remote['raw_codes'] = codes
                else:
                    self.error("{} unexpected".format(a[1]))
            elif a[0] == 'end':
                return remote
            else:
                k = a.pop(0)
                vals = []
                for v in a:
                    if v[0] == '#':
                        break
                    vals.append(int(v, 0))
                remote[k] = vals

    def read_codes(self):
        codes = {}
        while True:
            line = self.getline()
            if line == None:
                self.error("unexpected end of file")
                return None

            a = line.split()
            k = a.pop(0)
            if k == 'end':
                return codes
            if not k.startswith('KEY_'):
                k = 'KEY_' + k.upper()
            for s in a:
                if s[0] == '#':
                    break
                codes[int(s, 0)] = k

    def read_raw_codes(self):
        raw_codes = []
        codes = []
        name = ""
        while True:
            line = self.getline()
            if line == None:
                self.error("unexpected end of file")
                return None

            a = line.split()
            if a[0] == 'name':
                if len(codes) > 0:
                    raw_codes.append({ 'name': name, 'codes': codes })
                name = line.split(maxsplit=2)[1]
                if not name.startswith('KEY_'):
                    name = 'KEY_' + name.upper()
                codes = []
            elif a[0] == 'end':
                if len(codes) > 0:
                    raw_codes.append({ 'name': name, 'codes': codes })
                return raw_codes
            else:
                for v in a:
                    codes.append(int(v))


def eq_margin(duration, expected, margin):
    if duration >= (expected - margin) and duration <= (expected + margin):
        return True
    return False

def ffs(x):
    """Returns the index, counting from 0, of the
    least significant set bit in `x`.
    """
    return (x&-x).bit_length()-1

def rev8(v):
    r = 0
    for b in range(8):
        if v & (1 << (7 - b)):
            r |= 1 << b
    return r

def decode_nec_scancode(s):
    cmdc = rev8(s)
    cmd = rev8(s >> 8)
    addc = rev8(s >> 16)
    add = rev8(s >> 24)

    if cmd == (~cmdc & 0xff):
        if add == (~addc & 0xff):
            return 'nec', (add << 8) | cmd
        else:
            return 'necx', (add << 16) | (addc << 8) | cmd
    else:
        return 'nec32', (add << 24) | (addc << 16) | (cmd << 8) | cmdc

class Converter:
    def __init__(self, filename, remote):
        self.filename = filename
        self.remote = remote

    def error(self, msg):
        name = self.remote['name']
        print("{}:remote {}: error: {}".format(self.filename, name, msg), file=sys.stderr)

    def warning(self, msg):
        name = self.remote['name']
        print("{}:remote {}: warning: {}".format(self.filename, name, msg), file=sys.stderr)

    def convert(self):
        if 'driver' in self.remote:
            self.error("remote definition is for specific driver")
            return None

        if 'flags' not in self.remote:
            self.error("broken, missing flags parameter")
            return None

        flags = self.remote['flags']

        if 'rc5' in flags or 'shift_enc' in flags:
            return self.convert_rc5()
        elif 'rcmm' in flags:
            return self.convert_rcmm()
        elif 'space_enc' in flags:
            return self.convert_space_enc()
        else:
            self.error('Cannot convert remote with flags: {}'.format('|'.join(flags)))

        return None

    def convert_space_enc(self):
        if 'one' not in self.remote or 'zero' not in self.remote:
            self.error("broken, missing parameter for 'zero' and 'one'")
            return None

        res  = {
            'protocol': 'pulse_distance',
            'params': {},
            'map': {}
        }

        res['name'] = self.remote['name']

        if 'bits' not in self.remote:
            self.error("broken, missing 'bits' parameter")
            return None

        bits = int(self.remote['bits'][0])

        if 'footer' in self.remote:
            self.warning("cannot deal with parameter 'footer' yet")

        if 'ptrail' in self.remote:
            res['params']['trailer_pulse'] = self.remote['ptrail'][0]

        if 'slead' in self.remote:
            self.warning("cannot deal with parameter 'slead' yet")

        if 'no_foot_rep' in self.remote['flags']:
            self.warning("cannot deal with flag 'no_foot_rep' yet")

        if 'repeat_header' in self.remote['flags']:
            self.warning("cannot deal with flag 'repeat_header' yet")

        if 'no_head_rep' in self.remote['flags']:
            res['params']['header_optional'] = 1

        if 'reverse' in self.remote['flags']:
            res['params']['reverse'] = 1

        if 'header' in self.remote and self.remote['header'][0]:
            res['params']['header_pulse'] = self.remote['header'][0]
            res['params']['header_space'] = self.remote['header'][1]

        if 'repeat' in self.remote and self.remote['repeat'][0]:
            res['params']['repeat_pulse'] = self.remote['repeat'][0]
            res['params']['repeat_space'] = self.remote['repeat'][1]

        post_data_bits = 0
        if 'post_data_bits' in self.remote:
            post_data_bits = self.remote['post_data_bits'][0]

        pre_data = 0
        if 'pre_data_bits' in self.remote:
            pre_data_bits = int(self.remote['pre_data_bits'][0])
            if 'pre_data' in self.remote:
                pre_data = self.remote['pre_data'][0] << (bits + post_data_bits)
            bits += pre_data_bits

        if post_data_bits > 0:
            if 'post_data' in self.remote:
                pre_data |= self.remote['post_data'][0]
            bits += post_data_bits

        res['params']['bits'] = bits
        if eq_margin(self.remote['zero'][0], self.remote['one'][0], 100):
            res['params']['bit_pulse'] = int(self.remote['zero'][0])
            res['params']['bit_1_space'] = int(self.remote['one'][1])
            res['params']['bit_0_space'] = int(self.remote['zero'][1])
        elif eq_margin(self.remote['zero'][1], self.remote['one'][1], 100):
            res['params']['bit_space'] = int(self.remote['zero'][1])
            res['params']['bit_1_pulse'] = int(self.remote['one'][0])
            res['params']['bit_0_pulse'] = int(self.remote['zero'][0])
            res['protocol'] = 'pulse_length'
        else:
            self.error("unexpected combination of 'zero' and 'one'")
            return None

        # Many of these will be NEC; detect and convert
        if ('header_pulse' in res['params'] and
            'header_space' in res['params'] and
            'reverse' not in res['params'] and
            'trailer_pulse' in res['params'] and
            'header_optional' not in res['params'] and
            'pulse_distance' == res['protocol'] and
            eq_margin(res['params']['header_pulse'], 9000, 1000) and
            eq_margin(res['params']['header_space'], 4500, 1000) and
            eq_margin(res['params']['bit_pulse'], 560, 300) and
            eq_margin(res['params']['bit_0_space'], 560, 300) and
            eq_margin(res['params']['bit_1_space'], 1680, 300) and
            eq_margin(res['params']['trailer_pulse'], 560, 300) and
            res['params']['bits'] == 32 and
            ('repeat_pulse' not in res['params'] or
             (eq_margin(res['params']['repeat_pulse'], 9000, 1000) and
              eq_margin(res['params']['repeat_space'], 2250, 1000)))):
            self.warning('remote looks exactly like NEC, converting')
            res['protocol'] = 'nec'
            res['params'] = {}

            variant = None

            for s in self.remote['codes']:
                p = (s<<post_data_bits)|pre_data
                v, n = decode_nec_scancode(p)
                if variant == None:
                    variant = v
                elif v != variant:
                    variant = ""

                res['map'][n] = self.remote['codes'][s]

            if variant:
                res['params']['variant'] = "'" + variant + "'"
        else:
            for s in self.remote['codes']:
                p = (s<<post_data_bits)|pre_data
                res['map'][p] = self.remote['codes'][s]

        return res

    def convert_rcmm(self):
        res  = {
            'protocol': 'rc_mm',
            'params': {},
            'map': {}
        }

        res['name'] = self.remote['name']

        if 'bits' not in self.remote:
            self.error("broken, missing 'bits' parameter")
            return None

        bits = int(self.remote['bits'][0])

        toggle_bit = 0
        if 'toggle_bit_mask' in self.remote:
            toggle_bit = ffs(int(self.remote['toggle_bit_mask'][0]))
        if 'toggle_bit' in self.remote:
            toggle_bit = bits - int(self.remote['toggle_bit'][0])

        if toggle_bit > 0 and toggle_bit < bits:
            res['params']['toggle_bit'] = toggle_bit

        res['params']['bits'] = bits

        if 'codes' not in self.remote or len(self.remote['codes']) == 0:
            self.error("missing codes section")
            return None

        res['map'] = self.remote['codes']

        return res

    def convert_rc5(self):
        if 'one' not in self.remote or 'zero' not in self.remote:
            self.error("broken, missing parameter for 'zero' and 'one'")
            return None

        res  = {
            'protocol': 'manchester',
            'params': { },
            'map': { }
        }

        res['name'] = self.remote['name']

        if 'header' in self.remote and self.remote['header'][0]:
            res['params']['header_pulse'] = self.remote['header'][0]
            res['params']['header_space'] = self.remote['header'][1]

        if 'codes' not in self.remote or len(self.remote['codes']) == 0:
            self.error("missing codes section")
            return None

        bits = int(self.remote['bits'][0])

        toggle_bit = 0
        if 'toggle_bit_mask' in self.remote:
            toggle_bit = ffs(int(self.remote['toggle_bit_mask'][0]))
        if 'toggle_bit' in self.remote:
            toggle_bit = bits - int(self.remote['toggle_bit'][0])

        pre_data = 0
        if 'pre_data_bits' in self.remote:
            pre_data_bits = int(self.remote['pre_data_bits'][0])
            pre_data = int(self.remote['pre_data'][0]) << bits
            bits += pre_data_bits

        if 'plead' in self.remote:
            plead = self.remote['plead'][0]
            one_pulse = self.remote['one'][0]
            zero_pulse = self.remote['zero'][0]
            if eq_margin(plead, one_pulse, 200):
                res['params']['scancode_mask'] = 1 << bits
            elif not eq_margin(plead, one_pulse + zero_pulse, 200):
                self.error("plead has unexpected value")
                return None
            bits += 1

        if toggle_bit >= 0 and toggle_bit < bits:
            res['params']['toggle_bit'] = toggle_bit

        res['params']['bits'] = bits
        res['params']['zero_pulse'] = int(self.remote['zero'][0])
        res['params']['zero_space'] = int(self.remote['zero'][1])
        res['params']['one_pulse'] = int(self.remote['one'][0])
        res['params']['one_space'] = int(self.remote['one'][1])

        # Many of these will be regular RC5; detect this, and convert
        # the scancodes to ir-rc5-decoder.c format
        if ('header_pulse' not in res['params'] and
            'header_space' not in res['params'] and
            res['params']['bits'] == 14 and
            ('toggle_bit' in res['params'] and
             res['params']['toggle_bit'] == 11) and
            eq_margin(res['params']['one_pulse'], 888, 200) and
            eq_margin(res['params']['one_space'], 888, 200) and
            eq_margin(res['params']['zero_space'], 888, 200) and
            eq_margin(res['params']['zero_pulse'], 888, 200)):
            self.warning('remote looks exactly like regular RC-5, converting')
            res['params'] = {}
            res['protocol'] = 'rc5'
            newcodes = {}
            for s in self.remote['codes']:
                n = s|pre_data
                n = (n & 0x3f) | ((n << 2) & 0x1f00)
                newcodes[n] = self.remote['codes'][s]
            res['map'] = newcodes
        else:
            for s in self.remote['codes']:
                res['map'][s|pre_data] = self.remote['codes'][s]

        return res

def escapeString(s):
    return "'" + s.encode('unicode_escape').decode('utf-8') + "'"

def writeTOMLFile(fh, remote):
    print('[[protocols]]', file=fh)
    print('name = {}'.format(escapeString(remote['name'])), file=fh)
    print('protocol = {}'.format(escapeString(remote['protocol'])), file=fh)
    for p in remote['params']:
        print('{} = {}'.format(p, remote['params'][p]), file=fh)
    print('[protocols.scancodes]', file=fh)
    # find the largest scancode
    length=1
    for c in remote['map']:
        length=max(length, c.bit_length())

    # width seems to include '0x', hence the + 2
    width = math.ceil(length/4) + 2
    for c in remote['map']:
        print('{:#0{width}x} = {}'.format(c, escapeString(remote['map'][c]), width=width), file=fh)

    return True


parser = argparse.ArgumentParser(description="""Convert lircd.conf to rc-core toml format.
This program atempts to convert a lircd.conf remote definition to a
ir-keytable toml format. This process is not perfect, and the result
might need some tweaks for it to work. Please report any issues to
linux-media@vger,kernel,org. If you have successfully generated and
tested a toml keymap, please send it to the same mailinglist so it
can be include with the package.""")

parser.add_argument('input', metavar='INPUT', help='lircd.conf file')
parser.add_argument('-o', '--output', metavar='OUTPUT', help='toml output file')
parser.add_argument('--encoding', default='utf-8-sig', help='Encoding of lircd.conf')

args = parser.parse_args()

remoteNo=1
tomls=[]
remotes=LircdParser(args.input, args.encoding).parse()
if remotes == None:
    sys.exit(1)
for remote in remotes:
    if 'name' not in remote:
        remote['name'] = 'remote_{}'.format(remoteNo)
        remoteNo += 1
    lircRemote = Converter(args.input, remote)
    tomlRemote = lircRemote.convert()
    if tomlRemote != None:
        tomls.append(tomlRemote)

if len(tomls) == 0:
    print("{}: error: no convertible remotes found".format(args.input), file=sys.stderr)
else:
    fh=sys.stdout
    if args.output:
        try:
            fh = open(args.output, 'w')
        except IOError as e:
            print("{}: error: {}".format(filename, e), file=sys.stderr)
            sys.exit(2)
    for t in tomls:
        writeTOMLFile(fh, t)
