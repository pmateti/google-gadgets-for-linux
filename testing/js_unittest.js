/*
  Copyright 2007 Google Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

// JavaScript unittest framework.
// It's required to be run in a environment that provides the following
// global functions:
//   print([arg, ...]) : print one or more arguments to the standard output.
//   quit([code]) : quit current program with given exit code.
//   ASSERT(predicte_result[, message]) : do an assertion.
//   gc() : do JavaScript gc.
//
// General JavaScript unittest format:
// TEST(name, function() {
//   test_body;
//   ASSERT(Predicate(...)[, message]);
//   ...
//   END_TEST();
// }
// TEST(another_test, function() {
//   ...
// }
// 
// DO_ALL_TESTS();

const QUIT_JSERROR = -2;
const QUIT_ASSERT = -3;
const ASSERT_EXCEPTION_MAGIC = 135792468;

var _gAllTestCases = new Object();
var _gIsDeathTest = new Object();

// Define a test case.
// The test case body should include one or more ASSERTs and finish with
// END_TEST().
function TEST(case_name, test_function) {
  if (_gAllTestCases[case_name]) {
    print("Duplicate test case name: " + case_name);
    quit(QUIT_JSERROR);
  }
  _gAllTestCases[case_name] = test_function;
}

// Define a death test case that expects a failure.
function DEATH_TEST(case_name, test_function) {
  TEST(case_name, test_function);
  _gIsDeathTest[case_name] = true;
}

var _gCurrentTestFailed;

// Run all defined test cases.
function RUN_ALL_TESTS() {
  var count = 0;
  var passed = 0;
  for (var i in _gAllTestCases) {
    count++;
    print("Running " + (_gIsDeathTest[i] ? "death " : "") +
          "test case " + count + ": " + i + " . . .");
    _gCurrentTestFailed = !_gIsDeathTest[i];
    try {
      _gAllTestCases[i]();
    } catch (e) {
      // The exception thrown by ASSERT is of value ASSERT_EXCEPTION_MAGIC. 
      if (e !== ASSERT_EXCEPTION_MAGIC)
        print(e);
    }
    if (!_gCurrentTestFailed) {
      passed++;
      print((_gIsDeathTest[i] ? "Death test" : "Test") + " case " +
            count + ": " + i + " passed");
    }
  }
  print("\nSUMMARY\n");
  print(count + " test cases ran.");
  print(passed + " passed.");
  print((count - passed) + " failed.");
}

function END_TEST() {
  _gCurrentTestFailed = !_gCurrentTestFailed;
}

function _Message(expected, actual) {
  return "  Actual: " + actual + "\nExpected: " + expected;
}

// General format of ASSERT:
//   ASSERT(PREDICATE(args));
// or
//   ASSERT(PREDICATE(args), "message");
//
// The following are definitions of predicates. 

// Succeeds if arg is equivalent to true.
// Use EQ or STRICT_EQ instead to test if arg is equal to false.
function TRUE(arg) {
  return arg ? "" : _Message("true equivalent", arg);
}

// Succeeds if arg is equivalent to false.
// Use EQ or STRICT_EQ instead to test if arg is equal to false.
function FALSE(arg) {
  return !arg ? "" : _Message("false equivalent", arg);
}

function NULL(arg) {
  return arg == null ? "" : _Message("null", arg);
}

function NOT_NULL(arg) {
  return arg != null ? "" : _Message("not null", arg);
}

function UNDEFINED(arg) {
  return arg == undefined ? "" : _Message("undefined", arg);
}

function NOT_UNDEFINED(arg) {
  return arg != undefined ? "" : _Message("not undefined", arg);
}

function NAN(arg) {
  return isNaN(arg) ? "" : _Message("NaN", arg);
}

function NOT_NAN(arg) {
  return !isNaN(arg) ? "" : _Message("not NaN", arg);
}

function EQ(arg1, arg2) {
  return arg1 == arg2 ? "" : _Message(arg1, arg2);
}

function STRICT_EQ(arg1, arg2) {
  return arg1 === arg2 ? "" : _Message(arg1, arg2);
}

function NE(arg1, arg2) {
  return arg1 != arg2 ? "" : _Message("!=" + arg1, arg2);
}

function STRICT_NE(arg1, arg2) {
  return arg1 !== arg2 ? "" : _Message("!==" + arg1, arg2);
}

function LT(arg1, arg2) {
  return arg1 < arg2 ? "" : _Message("<" + arg1, arg2);
}

function LE(arg1, arg2) {
  return arg1 <= arg2 ? "" : _Message("<= " + arg1, arg2);
}

function GT(arg1, arg2) {
  return arg1 > arg2 ? "" : _Message(">" + arg1, arg2);
}

function GE(arg1, arg2) {
  return arg1 != arg2 ? "" : _Message(">=" + arg1, arg2);
}

function _ObjectEquals(array1, array2) {
  if (array1.length != array2.length)
    return false;
  for (var i = 0; i < array1.length; i++) {
    if (array1[i] != array2[i])
      return false;
  }
  return true;
}

function _ObjectStrictEquals(array1, array2) {
  if (array1.length != array2.length)
    return false;
  for (var i = 0; i < array1.length; i++) {
    if (array1[i] !== array2[i])
      return false;
  }
  return true;
}

function ARRAY_EQ(array1, array2) {
  return _ObjectEquals(array1, array2) ? "" :
         _Message("ARRAY==" + array1, array2);
}

function ARRAY_NE(array1, array2) {
  return !_ObjectEquals(array1, array2) ? "" :
         _Message("ARRAY!=" + array1, array2);
}
  
function ARRAY_STRICT_EQ(array1, array2) {
  return _ObjectStrictEquals(array1, array2) ? "" :
         _Message("ARRAY===" + array1, array2);
}

function ARRAY_STRICT_NE(array1, array2) {
  return !_ObjectStrictEquals(array1, array2) ? "" :
         _Message("ARRAY!==" + array1, array2);
}
  
function _ObjectEquals(object1, object2) {
  for (var i in object1) {
    if (object1[i] != object2[i])
      return false;
  }
  return true;
}

function _ObjectStrictEquals(object1, object2) {
  for (var i in object1) {
    if (object1[i] !== object2[i])
      return false;
  }
  return true;
}

function OBJECT_EQ(object1, object2) {
  return _ObjectEquals(object1, object2) ? "" :
         _Message("OBJECT==" + object1, object2);
}

function OBJECT_NE(object1, object2) {
  return !_ObjectEquals(object1, object2) ? "" :
         _Message("OBJECT!=" + object1, object2);
}
  
function OBJECT_STRICT_EQ(object1, object2) {
  return _ObjectStrictEquals(object1, object2) ? "" :
         _Message("OBJECT===" + object1, object2);
}

function OBJECT_STRICT_NE(object1, object2) {
  return !_ObjectStrictEquals(object1, object2) ? "" :
         _Message("OBJECT!==" + object1, object2);
}
