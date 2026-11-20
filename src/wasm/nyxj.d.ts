// TypeScript definitions for the NyxJ WebAssembly build (build/wasm/nyxj.js).
//
//   import NyxJ from './nyxj.js';
//   const m = await NyxJ();
//   const ptr = m.ccall('nyxj_wasm_format', 'number', ['string', 'number'], ['{"a":1}', 2]);
//   const res: NyxJResult<string> = JSON.parse(m.UTF8ToString(ptr));
//   m._nyxj_wasm_free(ptr);
//
// Every nyxj_wasm_* function returns a pointer to a heap JSON envelope that the
// caller releases with _nyxj_wasm_free. This surface is stable within a major
// version: functions may be added, existing signatures do not change.

export interface NyxJError {
  code: string; // e.g. "UNEXPECTED_EOF", see nyxj_error_name()
  message: string;
  line: number;
  column: number;
}

export type NyxJResult<T> = { ok: true; result: T } | { ok: false; error: NyxJError };

export type NyxJFunction =
  | 'nyxj_wasm_version'
  | 'nyxj_wasm_format'      // (json: string, indent: number) -> string
  | 'nyxj_wasm_minify'      // (json: string) -> string
  | 'nyxj_wasm_validate'    // (json: string) -> { type, size }
  | 'nyxj_wasm_query'       // (json: string, jsonpath: string) -> unknown[]
  | 'nyxj_wasm_repair'      // (text: string) -> { json, report }
  | 'nyxj_wasm_schema'      // (schema: string, data: string) -> { valid, errors }
  | 'nyxj_wasm_diff'        // (from: string, to: string) -> JSON Patch operations
  | 'nyxj_wasm_msgpack_size'; // (json: string) -> { json_bytes, msgpack_bytes }

export interface NyxJModule {
  ccall(name: NyxJFunction, returnType: 'number', argTypes: ('string' | 'number')[], args: (string | number)[]): number;
  cwrap(name: NyxJFunction, returnType: 'number', argTypes: ('string' | 'number')[]): (...args: (string | number)[]) => number;
  UTF8ToString(ptr: number): string;
  stringToNewUTF8(s: string): number;
  _nyxj_wasm_free(ptr: number): void;
  _malloc(size: number): number;
  _free(ptr: number): void;
}

declare function NyxJ(options?: Record<string, unknown>): Promise<NyxJModule>;
export default NyxJ;
