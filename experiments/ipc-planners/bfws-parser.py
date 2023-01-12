#! /usr/bin/env python

import re
import sys

from lab.parser import Parser

RUN_LOG_PATTERNS = [
    ('expansions', r'Nodes expanded during search: (\d+)', int),
    ('generations', r'Nodes generated during search: (\d+)', int),
    ('search_time', r'Fast-BFS search completed in (.+) secs', float),
]

class BFWSParser(Parser):
    def __init__(self):
        Parser.__init__(self)

        for name, pattern, typ in RUN_LOG_PATTERNS:
            self.add_pattern(name, pattern, type=typ)

def main():
    parser = BFWSParser()
    parser.parse()

main()
