oops-code - extract useful info from Linux kernel oops messages
Copyright (c) 2008, Bart Trojanowski <bart@jukie.net>

# About

This utility extracts useful information from kernel oops dumps.

Currently it's only job is to convert the `Code:` line, which is
a raw dump of the instructions around the crash, into assembly.
And it only works for i386.

## Source code

The project is tracked in git, you can obtain it by cloning it:

        git clone git://github.com/bartman/oops-code.git

## Dependencies

Other than gcc and libc, there are no build dependencies.  To run it you
will need `objdump` from the *binutils* package.

## Building

After obtaining the sources just run make in the project directory.

        make
        
# Using

        ./oops-code < oops-file
        # Code: 3b 74 24 10 77 03 c6 06 20 4d 46 85 ed 7f f1 e9 a0 00 00 00 8b 0f 81 f9 ff 0f 00 00 77 05 b9 37 60 29 c0 8b 54 24 18 89 c8 eb 06 <80> 38 00 74 07 40 4a 83 fa ff 75 f4 29 c8 f6 44 24 14 10 89 c3 
         

        /tmp/oops-code-9453:     file format elf32-i386

        Disassembly of section .oops:

        c01a516b <before>:
        c01a516b:	3b 74 24 10          	cmp    0x10(%esp),%esi
        c01a516f:	77 03                	ja     c01a5174 <before+0x9>
        c01a5171:	c6 06 20             	movb   $0x20,(%esi)
        c01a5174:	4d                   	dec    %ebp
        c01a5175:	46                   	inc    %esi
        c01a5176:	85 ed                	test   %ebp,%ebp
        c01a5178:	7f f1                	jg     c01a516b <before>
        c01a517a:	e9 a0 00 00 00       	jmp    c01a521f <oops+0x89>
        c01a517f:	8b 0f                	mov    (%edi),%ecx
        c01a5181:	81 f9 ff 0f 00 00    	cmp    $0xfff,%ecx
        c01a5187:	77 05                	ja     c01a518e <before+0x23>
        c01a5189:	b9 37 60 29 c0       	mov    $0xc0296037,%ecx
        c01a518e:	8b 54 24 18          	mov    0x18(%esp),%edx
        c01a5192:	89 c8                	mov    %ecx,%eax
        c01a5194:	eb 06                	jmp    c01a519c <oops+0x6>

        c01a5196 <oops>:
        c01a5196:	80 38 00             	cmpb   $0x0,(%eax)
        c01a5199:	74 07                	je     c01a51a2 <oops+0xc>
        c01a519b:	40                   	inc    %eax
        c01a519c:	4a                   	dec    %edx
        c01a519d:	83 fa ff             	cmp    $0xffffffff,%edx
        c01a51a0:	75 f4                	jne    c01a5196 <oops>
        c01a51a2:	29 c8                	sub    %ecx,%eax
        c01a51a4:	f6 44 24 14 10       	testb  $0x10,0x14(%esp)
        c01a51a9:	89 c3                	mov    %eax,%ebx




vim: set ts=8 et sw=8 tw=72 ft=mkd
