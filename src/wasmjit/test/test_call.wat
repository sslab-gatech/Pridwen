(module
  (type (;0;) (func (param i32 i32) (result i32)))
  (type (;1;) (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
  (type (;2;) (func (param i32) (result i32)))
  (type (;3;) (func (param i32 i32) (result i32)))
  (type (;4;) (func (param i32 i64) (result i64)))
  (type (;5;) (func (param i32 f32) (result f32)))
  (type (;6;) (func (param i32 f64) (result f64)))
  (type (;7;) (func (result i32)))
  (type (;8;) (func (result i64)))
  (type (;9;) (func (result f32)))
  (type (;10;) (func (result f64)))

  (memory (;0;) 2000 65536)
  (func (;0;) (type 0) (param i32 i32) (result i32)
    get_local 0
    get_local 1
    i32.add)
  (func (;1;) (type 0) (param i32 i32) (result i32)
    get_local 0
    get_local 1
    call 0)
  (func (;2;) (type 1) (param i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    get_local 7
    get_local 8
    i32.sub)
  (func (;3;) (type 1) (param i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    get_local 0
    get_local 1
    get_local 2
    get_local 3
    get_local 4
    get_local 5
    get_local 6
    get_local 7
    get_local 8
    call 2)
  (func (;4;) (type 2) (param i32) (result i32)
    (block
      (block
        (br_table
          0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1
          0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1
          0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1
          0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1
          (get_local 0)
        )
        (return (i32.const -1))
      )
      (return (i32.const 0))
    )
    (return (i32.const 1))
  )
  (func (;5;) (type 3) (param i32 i32) (result i32)
    get_local 0
    get_local 1
    i32.store
    get_local 0
    i32.load
  )
  (func (;6;) (type 4) (param i32 i64) (result i64)
    get_local 0
    get_local 1
    i64.store
    get_local 0
    i64.load
  )
  (func (;7;) (type 5) (param i32 f32) (result f32)
    get_local 0
    get_local 1
    f32.store
    get_local 0
    f32.load
  )
  (func (;8;) (type 6) (param i32 f64) (result f64)
    get_local 0
    get_local 1
    f64.store
    get_local 0
    f64.load
  )
  (func (;9;) (type 6) (param i32 f64) (result f64)
    get_local 0
    get_local 1
    call 8
  )
)
