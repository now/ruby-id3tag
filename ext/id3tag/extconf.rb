require 'mkmf'

def try_compiler_option(opt, &b)
  checking_for "‘#{opt}’ option to compiler" do
    if try_compile('', opt, &b)
      $CFLAGS += " #{opt}"
      true
    else
      false
    end
  end
end

have_header('id3tag.h')
have_library('id3tag', 'id3_tag_delete', 'id3tag.h')

try_compiler_option('-std=c99')
try_compiler_option('-finline-functions')
try_compiler_option('-Wall')
try_compiler_option('-Wextra')
try_compiler_option('-Wwrite-strings')
try_compiler_option('-Waggregate-return')
try_compiler_option('-Wmissing-prototypes')
try_compiler_option('-Wmissing-declarations')
try_compiler_option('-Wnested-externs')
try_compiler_option('-Wundef')
try_compiler_option('-Wpointer-arith')
try_compiler_option('-Wcast-align')
try_compiler_option('-Winline')
try_compiler_option('-Werror')

create_makefile('id3tag')
