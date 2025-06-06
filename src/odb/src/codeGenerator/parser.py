# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2021-2025, The OpenROAD Authors

import re


def _find_index(_list, _object, start_index=0):
    """Find the index of _object in _list[start_index:] ignoring white space"""
    index = start_index
    while index < len(_list):
        line = _list[index].strip()
        line = line.replace(" ", "")
        if line == _object:
            return index
        index += 1
    return -1


def _get_sections(lines, tag, sections=None, remove=False):
    """Find all sections delimeted by begin/end tags in lines.

    The lines containing the section tags are kept and not stored or deleted.
    Only the lines within by the section tags are operated on.

    If remove=False the lines found are stored in sections by tag name.
       Empty sections are not stored.
    If remove=True the lines found are deleted from lines
    """
    if sections is None:
        sections = {}
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        line = line.replace(" ", "")
        if re.match(tag, line):
            name = line[len(tag) :]

            if name in sections:
                raise Exception(f"Duplicate Tag: {name}")

            end = _find_index(lines, line.replace("Begin", "End", 1), i)
            if end == -1:
                raise Exception(f"Could not find an End for tag {name}\n")
            if remove:
                del lines[i + 1 : end]
            else:
                sections.setdefault(name, [])
                if end > i + 1:
                    sections[name].extend(lines[i + 1 : end])
        i += 1


class Parser:
    """Parses a file looking for the sections delimited by the code_tags."""

    def __init__(self, file_name):
        with open(file_name, "r", encoding="ascii") as file:
            self.lines = file.readlines()
        self.generator_code = {}
        self.user_code = {}
        self.set_comment_str("//")

    def set_comment_str(self, comment):
        """Set the prefix for the code tags comments

        For example, CMakeLists.txt uses # rather than //"""
        self.user_code_tag = f"{comment}UserCodeBegin"
        self.generator_code_tag = f"{comment}GeneratorCodeBegin"

    def parse_user_code(self):
        """Parse using the user_code_tag

        Used for a file where the default is generated code and user
        code is taggged."""
        _get_sections(self.lines, self.user_code_tag, self.user_code)

    def parse_source_code(self, file_name):
        """Parse using the file using the generator_code_tag

        Used for a file where the default is user code and generated
        code is taggged."""
        with open(file_name, "r", encoding="ascii") as source_file:
            db_lines = source_file.readlines()
        _get_sections(db_lines, self.generator_code_tag, self.generator_code)

    def clean_code(self):
        """Remove all section lines using the generator_code_tag

        Empties the generated sections in preparation for new code"""
        _get_sections(self.lines, self.generator_code_tag, remove=True)

    def write_in_file(self, file_name, keep_empty):
        """Write the file with user and/or generated sections"""
        # Replace the user sections inside the generate sections with
        # their current contents.
        for section in self.generator_code:
            db_lines = self.generator_code[section]
            j = 0
            while j < len(db_lines):
                line = db_lines[j].strip().replace(" ", "")
                if re.match(self.user_code_tag, line):
                    name = line[len(self.user_code_tag) :]
                    has_user_code = name in self.user_code
                    if has_user_code or keep_empty:
                        user = self.user_code.get(name, "")
                        if keep_empty or len(user) > 0:
                            db_lines[j + 1 : j + 1] = user
                            if has_user_code:
                                del self.user_code[name]
                        else:
                            db_lines[j : j + 2] = []
                    else:
                        db_lines[j : j + 2] = []
                j += 1
            self.generator_code[section] = db_lines

        # Ensure all non-empty user tags were used in the new generated
        # code.  We don't want to lose any previous user code accidentally.
        for tag in self.user_code:
            if len(self.user_code[tag]) > 0:
                raise Exception(f"User tag {tag} not used in {file_name}")

        # Replace the generated sections with their updated content
        i = 0
        while i < len(self.lines):
            line = self.lines[i].strip()
            line = line.replace(" ", "")
            if re.match(self.generator_code_tag, line):
                name = line[len(self.generator_code_tag) :]
                self.generator_code.setdefault(name, [])
                self.lines[i + 1 : i + 1] = self.generator_code[name]
            i += 1
        with open(file_name, "w", encoding="ascii") as out:
            out.write("".join(self.lines))
