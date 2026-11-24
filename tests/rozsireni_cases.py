"""Specification examples for the uploaded FUNEXP compiler plus VALMEM/PARAMLISTS."""
import math
import random

def program(body, definitions=''):
    return 'import ifj26.*\n'+definitions+'@main def main(): Unit =\n'+''.join('  '+line+'\n' for line in body.splitlines())

def extension_cases(paramlists=True):
    def good(group,name,body,output,definitions='',stdin=''):
        return dict(group=group,name=name,source=program(body,definitions),rc=0,output=output,stdin=stdin)
    def bad(group,name,body,rc,definitions=''):
        return dict(group=group,name=name,source=program(body,definitions),rc=rc,output='',stdin='')
    for name,expr,out in [
        ('string_length','"abcd".length','4'),('empty_length','"".length','0'),
        ('integer_toString','42.toString','42'),('zero_toString','0.toString','0'),
        ('unit_toString','().toString','()'),('string_toString','"abc".toString','abc'),
        ('double_toString','1.25.toString','1.25'),('fraction_toString','.5.toString','0.50'),
        ('exponent_toInt','1e2.toInt','100'),('suffix_toInt','1.25D.toInt','1'),
        ('negative_toInt','(-2.9).toInt','-2'),('negative_floor','(-2.9).floor.toInt','-3'),
        ('parenthesized','(1.5 + 2.75).floor.toString','4.00'),
        ('chained','(-2.9).floor.toInt.toString.length','2'),
        ('string_expression','("ab" + "cd").length','4'),
        ('member_in_expression','10 - "abc".length * 2','4'),
        ('call_receiver','text().length','3'),('grouped_call_receiver','(text()).length','3'),
        ('unit_call_receiver','empty().toString','()'),
    ]:
        defs='def text(): String = "abc"\ndef empty(): Unit = ()\n'
        yield good('valmem',name,'val x = '+expr+'\nprint(x)',out,defs)
    yield good('valmem','nested_argument','print(toStr(("a"+"bc").length+1))','4')
    yield good('valmem','readLine_member','print(readLine().length)','4',stdin='abc\n')
    yield good('valmem','receiver_evaluated_once','print(text().length)','T3','def text(): String =\n  print("T")\n  "abc"\n')
    yield good('valmem','loop_labels','var i = 0\nwhile i < 3 do\n  print((-1.25).floor.toInt)\n  i = i + 1','-2-2-2')
    rng=random.Random(26027)
    numbers=[0.,-0.,1.,-1.,.5,-.5,2147483648.5,-2147483648.5,5e-324,-5e-324]
    numbers += [rng.uniform(-1e6,1e6) for _ in range(80)]
    for i,x in enumerate(numbers):
        yield good('valmem',f'floor_{i:03}',f'print(({repr(x)}).floor.toInt)',str(math.floor(x)))
    for i,x in enumerate([2.**52,-(2.**52),2.**63,-(2.**63),1e100,-1e100]):
        literal=repr(x)
        yield good('valmem',f'large_floor_{i}',f'val x = ({literal}).floor\nif x == {literal} then\n  print("ok")\nelse\n  print("bad")','ok')
    for name,expr,rc in [('unknown','"x".other',3),('wrong_length','1.length',6),
            ('wrong_floor','1.floor',6),('wrong_toInt','"1".toInt',6),('missing_member','"x".',2),
            ('member_parentheses','"x".length()',2),('boolean_receiver','(1<2).toString',6),
            ('unknown_numeric_member','1._0',3)]:
        yield bad('valmem',name,'val x = '+expr,rc)
    # Recheck the existing FUNEXP directly; no new implementation replaces it.
    yield good('funexp','nested','print(add(add(1, 2), 4)*2)','14','def add(a: Int, b: Int): Int = a+b\n')
    yield good('funexp','forward','print(a()+1)','6','def a() = b()+1\ndef b() = 4\n')
    yield good('funexp','argument_order','print(add(tap(1), add(tap(2), tap(3))))','1236',
               'def tap(x: Int): Int =\n  print(x)\n  x\ndef add(a: Int, b: Int): Int = a+b\n')
    yield good('funexp','coercion','print(toStr(twice(1+2)))','6.00','def twice(x: Double): Double = x*2\n')
    yield bad('funexp','argument_type','print(chr("bad"))',5)
    yield bad('funexp','boolean_argument','print(1<2)',6)
    if not paramlists:return
    yield good('paramlists','specification','val x = answer\nval y = answerWithParens()\nval sum = add(x)(y)(1)\nval result = offset()(sum)\nprint(result)','86',
       'def answer: Int = 42\ndef answerWithParens(): Int = 42\ndef add(a: Int)(b: Int)(c: Int): Int = a+b+c\ndef offset()(value: Int): Int = value+1\n')
    yield good('paramlists','bare_argument','print(chr(code))','A','def code: Int = 65\n')
    yield good('paramlists','bare_expression','print(answer+answer)','84','def answer = 42\n')
    yield good('paramlists','empty_lists','print(add()()(2)()(3)())','5','def add()()(a: Int)()(b: Int)(): Int = a+b\n')
    yield good('paramlists','bare_member','print(answer.toString.length)','2','def answer = 42\n')
    yield good('paramlists','bare_unit','say\nsay','HH','def say: Unit = print("H")\n')
    yield good('paramlists','bare_repeated','print(tick+tick)','TT2','def tick: Int =\n  print("T")\n  1\n')
    yield good('paramlists','store_result','val x = tick\nprint(x+x)','T2','def tick: Int =\n  print("T")\n  1\n')
    yield good('paramlists','forward_bare','print(a)','5','def a = b+1\ndef b = 4\n')
    yield good('paramlists','forward_mix','print(a)','5','def a = b(2)\ndef b(x: Int) = c+x\ndef c = 3\n')
    yield good('paramlists','variable_shadow','val answer = 3\nprint(answer)','3','def answer = 42\n')
    yield good('paramlists','parameter_shadow','print(f(3))','3','def answer = 42\ndef f(answer: Int) = answer\n')
    yield good('paramlists','self_shadow_no_cycle','print(f)','3','def f =\n  val f = 3\n  f\n')
    yield good('paramlists','initializer_shadow','val answer = answer+1\nprint(answer)','43','def answer = 42\n')
    yield good('paramlists','block_shadow','if 1 == 1 then\n  val answer = 3\n  print(answer)\nelse\n  ()\nprint(answer)','342','def answer = 42\n')
    yield good('paramlists','member_no_cycle','print(length)','3','def length = "abc".length\n')
    yield good('paramlists','group_evaluation_order','print(sum(tap(1))(tap(2))(tap(3)))','1236',
        'def tap(x: Int): Int =\n  print(x)\n  x\ndef sum(a: Int)(b: Int)(c: Int): Int = a+b+c\n')
    yield good('paramlists','group_conversion','print(toStr(add(1)(2)))','3.00','def add(a: Double)(b: Double): Double = a+b\n')
    yield good('paramlists','multiline_trailing_comma','print(add(\n  1,\n  )(\n  2,\n  ))','3','def add(a: Int)(b: Int): Int = a+b\n')
    yield good('paramlists','empty_main','print(answer)','42','def answer = 42\n')
    for i in range(40):
        a,b,c=[rng.randrange(-100,100) for _ in range(3)]
        yield good('paramlists',f'grouped_{i:02}',f'print(add({a})({b})({c}))',str(a+b+c),'def add(a: Int)(b: Int)(c: Int): Int = a+b+c\n')
    for name,defs,body,rc in [
       ('partial','def f(a: Int)(b: Int) = a+b\n','val x = f(1)',5),
       ('flattened','def f(a: Int)(b: Int) = a+b\n','val x = f(1, 2)',5),
       ('wrong_split','def f(a: Int)(b: Int) = a+b\n','val x = f()(1, 2)',5),
       ('extra_list','def f(a: Int) = a\n','val x = f(1)()',5),
       ('missing_empty','def f()(a: Int) = a\n','val x = f(1)',5),
       ('bare_parentheses','def f = 1\n','val x = f()',5),
       ('missing_parentheses','def f() = 1\n','val x = f',5),
       ('duplicate_parameter','def f(x: Int)(x: Int) = x\n','()',4),
       ('wrong_type','def f(x: Int)(y: Double) = y\n','val x = f(1)("x")',5),
       ('bare_recursion','def f = f\n','()',6),
       ('bare_mutual_recursion','def f = g\ndef g = f\n','()',6),
       ('bare_mixed_recursion','def f: Int = g\ndef g = f\n','()',6),
       ('bare_nested_cycle','def f =\n  if 1==1 then\n    val f = 3\n    print(f)\n  else\n    ()\n  f\n','()',6),
       ('bare_return','def f =\n  return 1\n','()',6),
       ('not_a_function_value','def f = 1\n','val x = f\nx()',3),
       ('bare_builtin','','val x = readInt',5),
       ('same_line_trailing','def f(x: Int)(y: Int) = x+y\n','print(f(1,)(2))',2),
    ]:
        yield bad('paramlists',name,body,rc,defs)
    # A parameterless @main itself is a valid function under PARAMLISTS.
    yield dict(group='paramlists',name='main_without_list',source='import ifj26.*\n@main def main: Unit = print("ok")\n',rc=0,output='ok',stdin='')
