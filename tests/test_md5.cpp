/*
* Electra One MIDI Controller host tools
* See COPYRIGHT file at the top of the source tree.
*
* This product includes software developed by the
* Electra One Project (http://electra.one/).
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.
*/

#include <doctest/doctest.h>

// Internal header (not part of the public API) - electraone_tests gets -I
// src for exactly this kind of white-box access.
#include "md5.hpp"

// RFC 1321 test vectors, cross-checked against Python's hashlib during
// development.
TEST_CASE("md5Hex matches known RFC 1321 test vectors")
{
    CHECK(md5Hex(std::string("")) == "d41d8cd98f00b204e9800998ecf8427e");
    CHECK(md5Hex(std::string("a")) == "0cc175b9c0f1b6a831c399e269772661");
    CHECK(md5Hex(std::string("abc")) == "900150983cd24fb0d6963f7d28e17f72");
    CHECK(md5Hex(std::string("message digest"))
          == "f96b697d7cb7938d525a2f31aaf161d0");
    CHECK(md5Hex(std::string("abcdefghijklmnopqrstuvwxyz"))
          == "c3fcd3d76192e4007dfb496cca67e13b");
    CHECK(md5Hex(std::string("The quick brown fox jumps over the lazy dog"))
          == "9e107d9d372bb6826bd81d3542a419d6");
}

TEST_CASE("md5Hex(vector<uint8_t>) and md5Hex(string) agree")
{
    std::string s = "electra one";
    std::vector<uint8_t> bytes(s.begin(), s.end());
    CHECK(md5Hex(s) == md5Hex(bytes));
}

TEST_CASE("md5Hex output is always 32 lowercase hex characters")
{
    std::string digest = md5Hex(std::string("anything"));
    REQUIRE(digest.size() == 32);
    for (char c : digest) {
        CHECK(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')));
    }
}
