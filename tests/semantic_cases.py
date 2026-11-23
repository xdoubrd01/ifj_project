"""Generated semantic regression programs for base IFJ26."""

import textwrap

def program(body, helpers=""):
    helpers = textwrap.dedent(helpers).strip("\n")
    body = textwrap.indent(textwrap.dedent(body).strip("\n"), "    ")
    return ("import ifj26.*\n" + (helpers + "\n" if helpers else "")
            + "@main def run(): Unit =\n" + body + "\n")

TYPES = ('Int','Double','String','Unit')

VALUES = ('7','2.5','"text"','()')

BUILTINS = {
    'print': ('Any',), 'toStr': ('Any',), 'readInt': (), 'readDouble': (),
    'readLine': (), 'strLength': ('String',), 'chr': ('Int',),
    'toInt': ('Double',), 'ord': ('String','Int'),
    'subStr': ('String','Int','Int'),
}

def compatible(expected, actual):
    return expected == 'Any' or expected == actual or (expected,actual)==('Double','Int')

def cases():
    for ti,t in enumerate(TYPES):
        for vi,v in enumerate(TYPES):
            literal=VALUES[vi]
            ok=compatible(t,v)
            for keyword in ('val','var'):
                yield 'initialization',f'{keyword}_{t}_{v}',program(f'{keyword} x: {t} = {literal}'),0 if ok else 6
                yield 'assignment',f'{keyword}_{t}_{v}',program(f'{keyword} x: {t} = {VALUES[ti]}\nx = {literal}'),(0 if ok else 6) if keyword=='var' else 7
            for body in (literal,'\n    '+literal,'\n    return '+literal):
                yield 'returns',f'{t}_{v}_{len(body)}',program('f()',f'def f(): {t} ='+(' ' if not body.startswith('\n') else '')+body),0 if t=='Unit' or ok else 6
            yield 'arguments',f'user_{t}_{v}',program(f'use({literal})',f'def use(x: {t}): Unit = ()'),0 if ok else 5
            helpers=f'def value(): {v} = {literal}'
            yield 'call_initialization',f'{t}_{v}',program(f'val x: {t} = value()',helpers),0 if ok else 6
            yield 'call_assignment',f'{t}_{v}',program(f'var x: {t} = {VALUES[ti]}\nx = value()',helpers),0 if ok else 6
            yield 'call_return',f'{t}_{v}',program('wrap()',helpers+f'\ndef wrap(): {t} = value()'),0 if t=='Unit' or ok else 6
            for tail in ('return '+literal,literal):
                yield 'unreachable',f'{t}_{v}_{len(tail)}',program('f()',f'def f(): {t} =\n    return {VALUES[ti]}\n    {tail}'),0 if t=='Unit' or ok else 6
        yield 'parameters',f'immutable_{t}',program(f'f({VALUES[ti]})',f'def f(x: {t}): Unit =\n    x = {VALUES[ti]}'),7
        yield 'parameters',f'duplicate_{t}',program('()',f'def f(x: {t}, x: {t}): Unit = ()'),4
        yield 'parameters',f'redeclare_{t}',program('()',f'def f(x: {t}): Unit =\n    val x = {VALUES[ti]}'),4
        yield 'scopes',f'inner_shadow_{t}',program(f'f({VALUES[ti]})',f'def f(x: {t}): Unit =\n    if 1 == 1 then\n        val x = {VALUES[ti]}\n        print(x)\n    else\n        ()\n    print(x)'),0
        yield 'inference',f'literal_{t}',program('f()',f'def f() = {VALUES[ti]}'),0
        yield 'inference',f'forward_{t}',program('f()',f'def f() = g()\ndef g() = {VALUES[ti]}'),0
        chain='\n'.join(f'def f{i}() = f{i+1}()' for i in range(8))+f'\ndef f8() = {VALUES[ti]}'
        yield 'inference',f'chain_{t}',program('f0()',chain),0
        yield 'inference',f'annotated_return_required_{t}',program('()',f'def f() =\n    return {VALUES[ti]}'),6
        for ai,a in enumerate(TYPES):
            for bi,b in enumerate(TYPES):
                for explicit in (False,True):
                    keyword='return ' if explicit else ''
                    body=f'def f(): {t} =\n    if 1 == 1 then\n        {keyword}{VALUES[ai]}\n    else\n        {keyword}{VALUES[bi]}'
                    expected=0 if t=='Unit' or explicit and compatible(t,a) and compatible(t,b) else 6
                    yield 'branch_returns',f'{t}_{a}_{b}_{explicit}',program('f()',body),expected
    for op in ('+','-','*','/','<','<=','>','>=','==','!='):
        for ai,a in enumerate(TYPES):
            for bi,b in enumerate(TYPES):
                expression=f'{VALUES[ai]} {op} {VALUES[bi]}'
                numeric=a in ('Int','Double') and b in ('Int','Double')
                if op in ('+','-','*','/'):
                    expected=0 if numeric or op=='+' and a==b=='String' else 6
                    body='val x = '+expression
                else:
                    expected=0 if numeric or a==b=='String' or op in ('==','!=') and a==b=='Unit' else 6
                    body=f'if {expression} then\n    ()\nelse\n    ()'
                yield 'operators',f'{a}_{op}_{b}',program(body),expected
    for i,t in enumerate(TYPES):
        yield 'unary',t,program('val x = -'+VALUES[i]),0 if t in ('Int','Double') else 6
        for actual,literal in (('Int','-7'),('Double','-2.5')):
            yield 'signed_arguments',t+'_'+actual,program(f'use({literal})',f'def use(x: {t}): Unit = ()'),0 if compatible(t,actual) else 5
    for depth in (32,128,499,600):
        yield 'nested_expressions',str(depth),program('val x = '+'('*depth+'1'+')'*depth),0
    for name,signature in BUILTINS.items():
        baseline=[VALUES[TYPES.index(t)] if t!='Any' else VALUES[0] for t in signature]
        yield 'builtins',name+'_valid',program(name+'('+','.join(baseline)+')'),0
        for index,expected_type in enumerate(signature):
            for i,t in enumerate(TYPES):
                args=baseline.copy();args[index]=VALUES[i]
                yield 'builtins',f'{name}_{index}_{t}',program(name+'('+','.join(args)+')'),0 if compatible(expected_type,t) else 5
        for count in range(len(signature)+3):
            if count==len(signature): continue
            args=baseline[:count]+['1']*max(0,count-len(signature))
            yield 'arity',f'{name}_{count}',program(name+'('+','.join(args)+')'),5
        yield 'redefinition','builtin_'+name,program('()',f'def {name}(): Unit = ()'),4
    for count in range(7):
        args=['1','2.0','"s"','()'][:count]+['1']*max(0,count-4)
        yield 'arity',f'user_{count}',program('use('+','.join(args)+')','def use(a: Int,b: Double,c: String,d: Unit): Unit = ()'),0 if count==4 else 5
    for unknown in ('Missing','Bogus','Float','Any'):
        for keyword in ('val','var'):
            yield 'unknown_types',keyword+'_'+unknown,program(f'{keyword} x: {unknown} = 1'),3
        yield 'unknown_types','parameter_'+unknown,program('()',f'def f(x: {unknown}): Unit = ()'),3
        yield 'unknown_types','return_'+unknown,program('()',f'def f(): {unknown} = 1'),3
    for body in ('val x = missing','var x = missing','print(missing)','missing = 1','if missing == 1 then\n    ()\nelse\n    ()','while missing == 1 do\n    ()'):
        yield 'undefined','variable_'+str(len(body)),program(body),3
    yield 'undefined','return_variable',program('()','def f(): Int = missing'),3
    for name in ('missing','Missing','otherFunction'):
        yield 'undefined',name,program(name+'()'),3
    for value in VALUES:
        for kind in ('if','while'):
            body=f'if {value} then\n    ()\nelse\n    ()' if kind=='if' else f'while {value} do\n    ()'
            yield 'conditions',kind+value,program(body),6
    for condition in ('1 < 2','1 < 2.0','"a" < "b"','() == ()'):
        yield 'conditions','valid_'+condition,program(f'if {condition} then\n    ()\nelse\n    ()'),0
    for typed in ('Int','Double','String'):
        yield 'return_rules','bare_'+typed,program('()',f'def f(): {typed} =\n    return'),10
    yield 'return_rules','bare_unit',program('f()','def f(): Unit =\n    return'),0
    yield 'return_rules','unannotated_bare',program('()','def f() =\n    return'),6
    yield 'recursion','direct_inferred',program('()','def f() = f()'),6
    yield 'recursion','through_typed',program('()','def f() = g()\ndef g(): Unit = f()'),6
    yield 'recursion','typed_self',program('()','def f(): Unit = f()'),0
    yield 'scopes','self_initializer',program('val x = x'),3
    yield 'scopes','branch_escape',program('if 1 == 1 then\n    val hidden = 1\nelse\n    ()\nprint(hidden)'),3
    yield 'scopes','loop_escape',program('while 1 < 0 do\n    val hidden = 1\nprint(hidden)'),3
    yield 'main','missing','import ifj26.*\ndef f(): Unit = ()\n',3
    yield 'main','duplicate','import ifj26.*\n@main def a(): Unit = ()\n@main def b(): Unit = ()\n',4
    for t,v in zip(TYPES,VALUES):
        yield 'main','parameter_'+t,f'import ifj26.*\n@main def run(x: {t}): Unit = ()\n',5
        yield 'main','return_'+t,f'import ifj26.*\n@main def run(): {t} = {v}\n',0 if t=='Unit' else 6
