export interface Test {
  x: number;
  readonly y: number;
  functionOne(_0: number, _1: number): number;
  functionTwo(_0: number, _1: number): number;
  functionThree(_0: string|ArrayBuffer|Uint8Array|Uint8ClampedArray|Int8Array): number;
  functionFour(_0: boolean): number;
  functionFive(x: number, y: number): number;
  functionSix(str: string|ArrayBuffer|Uint8Array|Uint8ClampedArray|Int8Array): number;
  constFn(): number;
  delete(): void;
}

export interface BarValue<T extends number> {
  value: T;
}
export type Bar = BarValue<0>|BarValue<1>|BarValue<2>;

export interface EmptyEnumValue<T extends number> {
  value: T;
}
export type EmptyEnum = never/* Empty Enumerator */;

export type ValArr = [ number, number, number ];

export type ValArrIx = [ Bar, Bar, Bar, Bar ];

export interface IntVec {
  push_back(_0: number): undefined;
  resize(_0: number, _1: number): undefined;
  size(): number;
  set(_0: number, _1: number): boolean;
  get(_0: number): any;
  delete(): void;
}

export interface Foo {
  process(_0: Test): undefined;
  delete(): void;
}

export type ValObj = {
  foo: Foo,
  bar: Bar
};

export interface ClassWithConstructor {
  fn(_0: number): number;
  delete(): void;
}

export interface ClassWithSmartPtrConstructor {
  fn(_0: number): number;
  delete(): void;
}

export interface BaseClass {
  fn(_0: number): number;
  delete(): void;
}

export interface DerivedClass extends BaseClass {
  fn2(_0: number): number;
  delete(): void;
}

export interface MainModule {
  Test: {new(): Test; staticFunction(_0: number): number; staticFunctionWithParam(x: number): number; staticProperty: number};
  class_returning_fn(): Test;
  class_unique_ptr_returning_fn(): Test;
  an_int: number;
  a_bool: boolean;
  a_class_instance: Test;
  an_enum: Bar;
  Bar: {valueOne: BarValue<0>, valueTwo: BarValue<1>, valueThree: BarValue<2>};
  EmptyEnum: {};
  enum_returning_fn(): Bar;
  IntVec: {new(): IntVec};
  Foo: {new(): Foo};
  global_fn(_0: number, _1: number): number;
  ClassWithConstructor: {new(_0: number, _1: ValArr): ClassWithConstructor};
  ClassWithSmartPtrConstructor: {new(_0: number, _1: ValArr): ClassWithSmartPtrConstructor};
  smart_ptr_function(_0: ClassWithSmartPtrConstructor): number;
  smart_ptr_function_with_params(foo: ClassWithSmartPtrConstructor): number;
  function_with_callback_param(_0: (message: string) => void): number;
  BaseClass: {new(): BaseClass};
  DerivedClass: {new(): DerivedClass};
}
