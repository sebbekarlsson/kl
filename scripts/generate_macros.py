from contextlib import closing
import re
from textwrap import indent
import os

NAME: str = 'kl'
PREFIX: str = f'{NAME.upper()}_'

def strip(content: str) -> str:
    content = content.strip()
    while '\n\n' in content:
        content = content.replace('\n\n', '\n')
    return content
        
def escape_newline(content: str) -> str:
    lines = content.splitlines()
    lengths = list(map(lambda x: len(x), lines))
    maxlen = max(*lengths)
    nextlines: list[str] = []
    nlines = len(lines)
    for i, line in enumerate(lines):
        length = len(line)
        padlen = max(0, maxlen-length) + 2
        pad = ' ' * padlen
        
        nextlines.append(line + ((pad + '\\') if i < nlines-1 else '\n'))
    return '\n'.join(nextlines)
        

def process(src_path: str):
    content = open(src_path, 'r').read()
    content = content.replace('\t', '  ')
    content0 = content
    includes = list(re.finditer(r'#include.*?\n', content0))
    tops = list(re.finditer(r'(?s)//.*?<top>.*?</top>', content0))
    guards = list(re.finditer(r'#ifndef .*?\n#define .*?\n', content))
    content = content[guards[0].end():] if len(guards) > 0 else content
    content = ''.join(content.rsplit('#endif'))
    content = re.sub(r'(?s)//.*?<strip>.*?</strip>', '', content)
    

    for x in [*includes, *tops]:
        content = re.sub(x.re, '', content)


    content = re.sub(r'NAME', r'##NAME##', content)
    content = content.replace('(##NAME##', '(NAME##')#re.sub(r'\(NAME', '(NAME##', content) 
    content = re.sub(r'T_X', r'T', content)
    content = re.sub(r'(\s?.*?//.*?)(\n)', r'\2', content)
    
    return content, list(map(lambda xab: strip(content0[xab.start():xab.end()]), includes)), list(map(lambda xab: strip(content0[xab.start():xab.end()]), tops)) 

def generate(before_head: str, header_path: str, before_impl: str, impl_path: str, outpath: str):
    if not os.path.exists(header_path):
        print(f'No such file: {header_path}')
        return
    if not os.path.exists(impl_path):
        print(f'No such file: {impl_path}')
        return
    
    header, includes0, tops0 = process(header_path)
    impl, includes1, _tops1 = process(impl_path)
    all_includes = list(dict.fromkeys([*includes0, *includes1]))
    header = strip(header)
    impl = strip(impl)


    comments: list[str] = [
        'This file was automatically generated and must not be modified directly.',
    ]
    comment = '/*\n' +  '\n'.join(comments) + '\n*/'
    
    output = '\n'.join([
        comment,
        '\n',
        *all_includes,
        *tops0,
        '\n',
        escape_newline(''.join([
            before_head,
            indent(header, "  ")
        ])),
        '\n',
        escape_newline(''.join([
            before_impl,
            indent(impl, "  ")
        ]))
    ])
    with closing(open(outpath, 'w+')) as fp:
        fp.write(output + '\n/* generated */\n')
        print(f'Wrote to: {outpath}')


def generate_type(includes: list[str], template_name: str, typename: str, name: str, extra_impl: list[str] | None = None):
    nup = name.upper().replace(r'_t$', '')
    tup = template_name.upper()
    output_head = '\n'.join([
        f'#ifndef KL_{nup}_{template_name}_H',
        f'#define KL_{nup}_{template_name}_H',
        f'#include <{NAME}/templates/{template_name}/{template_name}.h>',
        *includes,
       f'\nKL_DEFINE_{tup}({name}, {typename})\n',
       '#endif'
    ])
    # extra_impl lets a template instantiate the helper containers it composes
    # (e.g. unique_list embeds a `<name>__u` list whose KL_IMPL_LIST must be
    # emitted alongside KL_IMPL_<template>). Lines may reference {name}/{typename}.
    extra_impl_lines = [line.format(name=name, typename=typename) for line in (extra_impl or [])]
    output_impl = '\n'.join([
        f'#include <{NAME}/templates/{template_name}/{name}_{template_name}.h>',
        *extra_impl_lines,
        f'KL_IMPL_{tup}({name}, {typename})',
    ])
    headpath = f'./include/{NAME}/templates/{template_name}/{name}_{template_name}.h'
    implpath = f'./src/templates/{template_name}/{name}_{template_name}.c'
    with closing(open(headpath, 'w+')) as fp:
        fp.write(output_head + '\n/* generated */\n')
        print(f'Wrote to: {headpath}')

    with closing(open(implpath, 'w+')) as fp:
        fp.write(output_impl + '\n/* generated */\n')
        print(f'Wrote to: {implpath}')
    pass


if __name__ == '__main__':
    # list
    generate(
        before_head=f'#define {PREFIX}DEFINE_LIST(NAME, T)\n',
        header_path=f'./include/{NAME}/templates/list/dummy.h',
        before_impl=f'#define {PREFIX}IMPL_LIST(NAME, T)\n',
        impl_path='./src/templates/list/dummy.c',
        outpath=f'./include/{NAME}/templates/list/list.h'
    )
    generate_type(
        includes=['#include <stdint.h>'],
        template_name='list',
        typename='int64_t',
        name='int64'
    )
    generate_type(
        includes=['#include <stdint.h>'],
        template_name='list',
        typename='uint32_t',
        name='uint32'
    )
    generate_type(
        includes=[],
        template_name='list',
        typename='float',
        name='float'
    )
    generate_type(
        includes=[],
        template_name='list',
        typename='float',
        name='f32'
    )
    generate_type(
        includes=[],
        template_name='list',
        typename='double',
        name='f64'
    )
    generate_type(
        includes=[f'#include <{NAME}/binary/blob.h>'],
        template_name='list',
        typename=f'{PREFIX}Blob*',
        name='blob_ptr'
    )
    generate_type(
        includes=[f'#include <{NAME}/common/int64_pair.h>'],
        template_name='list',
        typename=f'{PREFIX}Int64Pair',
        name='int64_pair'
    )
    generate_type(
        includes=[f'#include <{NAME}/string/string.h>'],
        template_name='list',
        typename=f'{PREFIX}StrConst',
        name='str_const'
    )

    # unique_list
    generate(
        before_head=f'#define {PREFIX}DEFINE_UNIQUE_LIST(NAME, T)\n',
        header_path=f'./include/{NAME}/templates/unique_list/dummy.h',
        before_impl=f'#define {PREFIX}IMPL_UNIQUE_LIST(NAME, T)\n',
        impl_path='./src/templates/unique_list/dummy.c',
        outpath=f'./include/{NAME}/templates/unique_list/unique_list.h'
    )
    generate_type(
        includes=['#include <stdint.h>'],
        template_name='unique_list',
        typename='int64_t',
        name='int64',
        extra_impl=['KL_IMPL_LIST({name}__u, {typename})'],
    )

    # queue
    generate(
        before_head=f'#define {PREFIX}DEFINE_QUEUE(NAME, T)\n',
        header_path=f'./include/{NAME}/templates/queue/dummy.h',
        before_impl=f'#define {PREFIX}IMPL_QUEUE(NAME, T)\n',
        impl_path='./src/templates/queue/dummy.c',
        outpath=f'./include/{NAME}/templates/queue/queue.h'
    )

    # hashmap
    generate(
        before_head=f'#define {PREFIX}DEFINE_HASHMAP(NAME, T)\n',
        header_path=f'./include/{NAME}/templates/hashmap/dummy.h',
        before_impl=f'#define {PREFIX}IMPL_HASHMAP(NAME, T)\n',
        impl_path='./src/templates/hashmap/dummy.c',
        outpath=f'./include/{NAME}/templates/hashmap/hashmap.h'
    )
    generate_type(
        includes=['#include <stdint.h>'],
        template_name='hashmap',
        typename='int64_t',
        name='int64'
    )
    generate_type(
        includes=['#include <stdint.h>'],
        template_name='hashmap',
        typename='uint64_t',
        name='uint64'
    )
    generate_type(
        includes=['#include <stdint.h>'],
        template_name='hashmap',
        typename='uint32_t',
        name='uint32'
    )
    generate_type(
        includes=['#include <stdbool.h>'],
        template_name='hashmap',
        typename='bool',
        name='bool'
    )
    generate_type(
        includes=[f'#include <{NAME}/templates/list/int64_list.h>'],
        template_name='hashmap',
        typename='KL_int64_List',
        name='int64_list'
    )


    # sparse_set
    generate(
        before_head=f'#define {PREFIX}DEFINE_SPARSE_SET(NAME, T)\n',
        header_path=f'./include/{NAME}/templates/sparse_set/dummy.h',
        before_impl=f'#define {PREFIX}IMPL_SPARSE_SET(NAME, T)\n',
        impl_path='./src/templates/sparse_set/dummy.c',
        outpath=f'./include/{NAME}/templates/sparse_set/sparse_set.h'
    )
    generate_type(
        includes=['#include <stdint.h>'],
        template_name='sparse_set',
        typename='int64_t',
        name='i64'
    )


    # ringbuffer 
    generate(
        before_head=f'#define {PREFIX}DEFINE_RINGBUFFER(NAME, T)\n',
        header_path=f'./include/{NAME}/templates/ringbuffer/dummy.h',
        before_impl=f'#define {PREFIX}IMPL_RINGBUFFER(NAME, T)\n',
        impl_path='./src/templates/ringbuffer/dummy.c',
        outpath=f'./include/{NAME}/templates/ringbuffer/ringbuffer.h'
    )
    generate_type(
        includes=[],
        template_name='ringbuffer',
        typename='float',
        name='f32'
    )
    generate_type(
        includes=[],
        template_name='ringbuffer',
        typename='double',
        name='f64'
    )
