#include "isa_parser.h"

#include <cassert>

int main()
{
  const isa_parser_t ventus("rv32ima_zfinx_zve32f", "m");
  assert(ventus.get_max_xlen() == 32);
  assert(ventus.extension_enabled(EXT_ZFINX));
  assert(ventus.extension_enabled(EXT_ZVE32X));
  assert(ventus.extension_enabled(EXT_ZVE32F));
  assert(ventus.extension_enabled('V'));
  assert(!ventus.extension_enabled('F'));
  assert(!ventus.extension_enabled('D'));

  const isa_parser_t standard("rv32if_zve32f", "m");
  assert(!standard.extension_enabled(EXT_ZFINX));
  assert(standard.extension_enabled(EXT_ZVE32X));
  assert(standard.extension_enabled(EXT_ZVE32F));
  assert(standard.extension_enabled('V'));
  assert(standard.extension_enabled('F'));
  assert(!standard.extension_enabled('D'));

  return 0;
}
