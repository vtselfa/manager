#!/usr/bin/env python3

import sys
import yaml
import argparse
from mako.template import Template
from mako.lookup import TemplateLookup


def main():
    parser = argparse.ArgumentParser(description = 'Line plot of csv files.')
    parser.add_argument('template', help='Mako template.')
    parser.add_argument('--lookup', default="templates", help='Lookup dir.')
    parser.add_argument('--defs', type=yaml.load, help='Dict with variables passed to the temlate.', default=dict())
    args = parser.parse_args()

    lookup = TemplateLookup(directories=[args.lookup])
    template = Template(filename=args.template, lookup=lookup)
    print(template.render(**args.defs))



if __name__ == "__main__":
        main()

