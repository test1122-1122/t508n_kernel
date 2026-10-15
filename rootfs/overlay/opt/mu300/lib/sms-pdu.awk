# SMS PDU codec for the MU300. Decodes SMS-DELIVER and builds SMS-SUBMIT.
#
# In awk because that is what the device has: no python, no lua, no perl, only an awk - busybox's on OpenWrt,
# mawk on Ubuntu. printf "%c" with a value under 256 emits a raw byte in all of them, so UTF-8 is written out a
# byte at a time. The bit operations are the b_* functions below rather than b_and()/b_or()/b_lshift()/b_rshift():
# busybox awk and gawk have those built in, mawk does not, and on Ubuntu every USSD answer and every SMS
# failed with "function and never defined". gawk will not let a script define its own b_and(), hence the prefix.
#
# PDU mode rather than text mode is not a preference. This modem returns a mangled address in text mode
# whenever the sender is alphanumeric - a brand name rather than a number, which is most of what a SIM in a
# router receives. Measured: text mode gives "144414+4140224P444" where the PDU says type-of-address 0xD0 and
# the packed septets spell the operator's name. Text mode also cannot say whether two messages are halves of
# one long one. See docs/FINDINGS.md.
#
#   decode: awk -v mode=decode -f sms-pdu.awk        (one PDU per line, "index<TAB>pdu")
#   encode: awk -v mode=encode -v number=... -v text=... -f sms-pdu.awk
#
# Decode prints one record per line, tab separated and with tabs and newlines escaped inside fields:
#   index <TAB> sender <TAB> timestamp <TAB> ref <TAB> total <TAB> part <TAB> text

# Bit operations on non-negative integers, in plain arithmetic so that every awk runs them (see the top).
function b_and(a, b,    r, p) {
    r = 0; p = 1
    while (a > 0 && b > 0) { if (a % 2 == 1 && b % 2 == 1) r += p; a = int(a / 2); b = int(b / 2); p *= 2 }
    return r
}
function b_or(a, b,    r, p) {
    r = 0; p = 1
    while (a > 0 || b > 0) { if (a % 2 == 1 || b % 2 == 1) r += p; a = int(a / 2); b = int(b / 2); p *= 2 }
    return r
}
# no "^": busybox awk built without math support rejects it ("Math support is not compiled in")
function b_lshift(a, n) { while (n-- > 0) a *= 2; return a }
function b_rshift(a, n) { while (n-- > 0) a = int(a / 2); return a }

function hexval(c) {
    c = toupper(c)
    if (c >= "0" && c <= "9") return index("0123456789", c) - 1
    return index("ABCDEF", c) + 9
}
function byteat(h, i,    a, b) {        # i is 0-based byte index
    a = substr(h, i * 2 + 1, 1); b = substr(h, i * 2 + 2, 1)
    return hexval(a) * 16 + hexval(b)
}

# ---- GSM 03.38 default alphabet, as UTF-8 code points ----------------------
function gsm_init(    i, s) {
    split("64,163,36,165,232,233,249,236,242,199,10,216,248,13,197,229," \
          "916,95,934,915,923,937,928,936,931,920,926,27,198,230,223,201," \
          "32,33,34,35,164,37,38,39,40,41,42,43,44,45,46,47," \
          "48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63," \
          "161,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79," \
          "80,81,82,83,84,85,86,87,88,89,90,196,214,209,220,167," \
          "191,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111," \
          "112,113,114,115,116,117,118,119,120,121,122,228,246,241,252,224", GSM, ",")
    # escape sequences: 0x1B followed by these
    EXT[10] = 12; EXT[20] = 94; EXT[40] = 123; EXT[41] = 125; EXT[47] = 92
    EXT[60] = 91; EXT[61] = 126; EXT[62] = 93; EXT[64] = 124; EXT[101] = 8364
    for (i = 0; i < 128; i++) RGSM[GSM[i + 1] + 0] = i
    for (s in EXT) RESC[EXT[s] + 0] = s + 0

    # Turkish national language tables (3GPP TS 23.038 A.3.1). A Turkish sender's message arrives as ordinary
    # GSM 7-bit with a header saying "read it against this table instead", and without that the text comes out
    # with the right shape and the wrong letters: "hastasi" turns into "hastasì", "bagista" into "baøìæta".
    # Eight positions move, and they are exactly the Turkish ones.
    TR[4] = 8364; TR[7] = 305; TR[11] = 286; TR[12] = 287
    TR[28] = 350; TR[29] = 351; TR[64] = 304; TR[96] = 231
    # the Turkish single shift table, reached through ESC
    TRX[20] = 94; TRX[40] = 123; TRX[41] = 125; TRX[47] = 92; TRX[60] = 91
    TRX[61] = 126; TRX[62] = 93; TRX[64] = 124; TRX[101] = 8364
    TRX[71] = 286; TRX[73] = 304; TRX[83] = 350; TRX[99] = 231; TRX[103] = 287
    TRX[105] = 305; TRX[115] = 351
}
# the code point for one septet, honouring whichever national table the header asked for
function septet_char(sept, lang) {
    if (lang == 1 && (sept in TR)) return TR[sept]
    return GSM[sept + 1]
}
function septet_esc(sept, langx) {
    if (langx == 1 && (sept in TRX)) return TRX[sept]
    if (sept in EXT) return EXT[sept]
    return GSM[sept + 1]
}
# UTF-8 for one code point
function utf8(c) {
    if (c < 128) return sprintf("%c", c)
    if (c < 2048) return sprintf("%c%c", 192 + int(c / 64), 128 + (c % 64))
    return sprintf("%c%c%c", 224 + int(c / 4096), 128 + int(c / 64) % 64, 128 + (c % 64))
}

# ---- decoding --------------------------------------------------------------
# septets packed into octets, starting at byte offset `off`, `n` septets, skipping `skip` of them
function gsm7_decode(h, off, n, skip, lang, langx,    i, bit, byte, sept, out, esc, c) {
    out = ""; esc = 0
    for (i = 0; i < n; i++) {
        bit = i * 7
        byte = int(bit / 8)
        sept = b_rshift(byteat(h, off + byte), bit % 8)
        if ((bit % 8) > 1) sept = b_or(sept, b_lshift(byteat(h, off + byte + 1), 8 - (bit % 8)))
        sept = b_and(sept, 127)
        if (i < skip) continue
        if (sept == 27) { esc = 1; continue }
        if (esc) { esc = 0; out = out utf8(septet_esc(sept, langx)); continue }
        out = out utf8(septet_char(sept, lang))
    }
    return out
}
function ucs2_decode(h, off, n,    i, c, out) {
    out = ""
    for (i = 0; i < n; i += 2) out = out utf8(byteat(h, off + i) * 256 + byteat(h, off + i + 1))
    return out
}
# semi-octet (BCD) telephone number, `digits` digits starting at byte `off`
function bcd_decode(h, off, digits,    i, b, lo, hi, out) {
    out = ""
    for (i = 0; i * 2 < digits; i++) {
        b = byteat(h, off + i)
        lo = b_and(b, 15); hi = b_rshift(b, 4)
        out = out substr("0123456789*#abc", lo + 1, 1)
        if (i * 2 + 1 < digits) out = out substr("0123456789*#abc", hi + 1, 1)
    }
    return out
}
function ts_decode(h, off,    i, b, d, p) {
    d = ""
    for (i = 0; i < 6; i++) {
        b = byteat(h, off + i)
        d = d substr("0123456789", b_and(b, 15) + 1, 1) substr("0123456789", b_rshift(b, 4) + 1, 1)
        if (i == 2) d = d " "; else if (i < 2) d = d "-"; else if (i < 5) d = d ":"
    }
    return "20" d
}
function esc_field(s) { gsub(/\t/, " ", s); gsub(/\n/, " ", s); gsub(/\r/, "", s); return s }

function decode_pdu(idx, h,    i, smsc, first, oal, toa, oabytes, sender, pid, dcs, ts, udl, udhl, \
                    ref, total, part, off, skipsept, text, alpha, j, hend, iei, ielen, lang, langx) {
    i = 0
    smsc = byteat(h, i); i += 1 + smsc
    first = byteat(h, i); i += 1
    if (b_and(first, 3) != 0) return ""        # not an SMS-DELIVER
    oal = byteat(h, i); i += 1
    toa = byteat(h, i); i += 1
    oabytes = int((oal + 1) / 2)
    alpha = (b_and(toa, 112) == 80)
    if (alpha) sender = gsm7_decode(h, i, int(oal * 4 / 7), 0, 0, 0)
    else       sender = (b_and(toa, 112) == 16 ? "+" : "") bcd_decode(h, i, oal)
    i += oabytes
    pid = byteat(h, i); i += 1
    dcs = byteat(h, i); i += 1
    ts = ts_decode(h, i); i += 7
    udl = byteat(h, i); i += 1

    ref = ""; total = 1; part = 1; udhl = 0; lang = 0; langx = 0
    if (b_and(first, 64)) {                    # UDHI: a header sits in front of the text
        udhl = byteat(h, i)
        # walk every element; the concatenation one is not always first, and the language one never is
        j = i + 1; hend = i + 1 + udhl
        while (j + 1 < hend) {
            iei = byteat(h, j); ielen = byteat(h, j + 1)
            if (iei == 0)      { ref = byteat(h, j + 2); total = byteat(h, j + 3); part = byteat(h, j + 4) }
            else if (iei == 8) { ref = byteat(h, j + 2) * 256 + byteat(h, j + 3); total = byteat(h, j + 4); part = byteat(h, j + 5) }
            else if (iei == 36) langx = byteat(h, j + 2)     # 0x24 national language single shift
            else if (iei == 37) lang  = byteat(h, j + 2)     # 0x25 national language locking shift
            j += 2 + ielen
        }
    }
    off = i + (udhl ? udhl + 1 : 0)
    if (b_and(dcs, 12) == 8) {                             # UCS2
        text = ucs2_decode(h, off, udl - (udhl ? udhl + 1 : 0))
    } else if (b_and(dcs, 12) == 4) {                      # 8-bit
        text = ""
        for (i = 0; i < udl - (udhl ? udhl + 1 : 0); i++) text = text utf8(byteat(h, off + i))
    } else {                                             # GSM 7-bit, septet aligned past the header
        skipsept = udhl ? int(((udhl + 1) * 8 + 6) / 7) : 0
        text = gsm7_decode(h, i, udl, skipsept, lang, langx)
    }
    return esc_field(idx) "\t" esc_field(sender) "\t" ts "\t" ref "\t" total "\t" part "\t" esc_field(text)
}

# ---- encoding --------------------------------------------------------------
function bcd_encode(num,    i, out, d, n) {
    gsub(/[^0-9]/, "", num)
    n = length(num)
    out = ""
    for (i = 1; i <= n; i += 2) {
        d = substr(num, i + 1, 1); if (d == "") d = "F"
        out = out d substr(num, i, 1)
    }
    return out
}
# UTF-8 in, array of code points out; returns the count
function utf8_points(s, cp,    i, b, c, n, extra, val) {
    n = 0; i = 1
    while (i <= length(s)) {
        b = index_of_byte(s, i)
        if (b < 128) { val = b; extra = 0 }
        else if (b < 224) { val = b_and(b, 31); extra = 1 }
        else if (b < 240) { val = b_and(b, 15); extra = 2 }
        else { val = b_and(b, 7); extra = 3 }
        for (c = 1; c <= extra; c++) { i++; val = val * 64 + b_and(index_of_byte(s, i), 63) }
        cp[++n] = val
        i++
    }
    return n
}
function index_of_byte(s, i) { return ORD[substr(s, i, 1)] + 0 }
function ord_init(    i) { for (i = 0; i < 256; i++) ORD[sprintf("%c", i)] = i }

function gsm7_encodable(cp, n,    i) {
    for (i = 1; i <= n; i++) if (!((cp[i] + 0) in RGSM) && !((cp[i] + 0) in RESC)) return 0
    return 1
}
function gsm7_pack(cp, n,    i, sept, k, bit, byte, bits, out, val, hi) {
    k = 0
    for (i = 1; i <= n; i++) {
        val = cp[i] + 0
        if (val in RGSM) sept[k++] = RGSM[val]
        else { sept[k++] = 27; sept[k++] = RESC[val] }
    }
    SEPTETS = k
    out = ""; bits = 0; val = 0
    for (i = 0; i < k; i++) {
        val = b_or(val, b_lshift(sept[i], bits)); bits += 7
        while (bits >= 8) { out = out sprintf("%02X", b_and(val, 255)); val = b_rshift(val, 8); bits -= 8 }
    }
    if (bits > 0) out = out sprintf("%02X", b_and(val, 255))
    return out
}
function ucs2_pack(cp, n,    i, out, v) {
    out = ""
    for (i = 1; i <= n; i++) { v = cp[i] + 0; out = out sprintf("%02X%02X", int(v / 256), v % 256) }
    UNITS = n * 2
    return out
}

BEGIN {
    gsm_init(); ord_init()
    # A +CUSD body, which is not a PDU: with AT+CSCS="HEX" the modem converts the answer to characters and then
    # hex-dumps them, one byte per character, so there are no packed septets to unpick here. Measured on this
    # modem - eleven bytes for an eleven-character answer. Only the alphabet is in question, and the CBS coding
    # scheme in the third +CUSD field says which: bits 3-2 == 10 means UCS2, anything else the GSM alphabet.
    if (mode == "ussd") {
        n = length(hex) / 2
        if (b_and(dcs + 0, 12) == 8) { print ucs2_decode(hex, 0, n); exit }
        out = ""; esc = 0
        for (i = 0; i < n; i++) {
            c = byteat(hex, i)
            if (c > 127) { out = out utf8(c); continue }   # not the GSM alphabet; Latin-1 is the best guess left
            if (c == 27) { esc = 1; continue }
            if (esc) { esc = 0; out = out utf8(septet_esc(c, 0)); continue }
            out = out utf8(septet_char(c, 0))
        }
        print out
        exit
    }
    if (mode == "encode") {
        n = utf8_points(text, cp)
        if (gsm7_encodable(cp, n)) { ud = gsm7_pack(cp, n); dcs = "00"; udl = SEPTETS }
        else                       { ud = ucs2_pack(cp, n); dcs = "08"; udl = UNITS }
        num = number
        intl = (substr(num, 1, 1) == "+")
        gsub(/[^0-9]/, "", num)
        # 00 = no service centre in the PDU, use the one the SIM already knows.
        # 11 = SMS-SUBMIT with a relative validity period; AA after the DCS is four days.
        tpdu = "11" "00" sprintf("%02X", length(num)) (intl ? "91" : "81") bcd_encode(num) \
               "00" dcs "AA" sprintf("%02X", udl) ud
        # AT+CMGS wants the length of the TPDU alone, in octets - the leading 00 does not count
        printf "%d %s\n", length(tpdu) / 2, "00" tpdu
        exit
    }
}
mode == "decode" && NF >= 2 {
    line = decode_pdu($1, $2)
    if (line != "") print line
}
