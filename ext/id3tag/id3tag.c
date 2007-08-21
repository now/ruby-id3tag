/*
 * contents: unknown
 *
 * Copyright © 2005 Nikolai Weibull <nikolai@bitwi.se>
 */

#include <ruby.h>
#include <id3tag.h>

static VALUE cID3Frame;
static VALUE cID3Tag;

static struct id3_frame *
value2frame(VALUE self)
{
        struct id3_frame *frame;

        Data_Get_Struct(self, struct id3_frame, frame);

        return frame;
}

static VALUE
frame_allocate(VALUE klass)
{
        return Data_Wrap_Struct(klass, NULL, id3_frame_delete, NULL);
}

static VALUE
frame_initialize(VALUE self, VALUE id)
{
        DATA_PTR(self) = id3_frame_new(StringValuePtr(id));
        
        return self;
}

static VALUE
frame_field_type(VALUE self, VALUE n)
{
        return INT2FIX(id3_field_type(id3_frame_field(value2frame(self), FIX2UINT(n))));
}

static VALUE
frame_type(VALUE self)
{
        return rb_str_new2(value2frame(self)->id);
}

static VALUE
frame_description(VALUE self)
{
        return rb_str_new2(value2frame(self)->description);
}

static VALUE
str_new_ucs4(id3_ucs4_t const *input)
{
        id3_utf8_t *utf8_input = id3_ucs4_utf8duplicate(input);
        VALUE str = rb_str_new2((char *)utf8_input);
        free(utf8_input);
        return str;
}

static struct {
        enum id3_field_textencoding encoding;
        char const *id;
} encodings[] = {
        { ID3_FIELD_TEXTENCODING_ISO_8859_1, "iso-8859-1" },
        { ID3_FIELD_TEXTENCODING_UTF_16, "utf-16" },
        { ID3_FIELD_TEXTENCODING_UTF_16BE, "utf-16be" },
        { ID3_FIELD_TEXTENCODING_UTF_8, "utf-8" },
};

static VALUE
frame_field_get_textencoding(union id3_field *field)
{
        enum id3_field_textencoding e = id3_field_gettextencoding(field);

        for (unsigned int i = 0; i < sizeof(encodings) / sizeof(encodings[0]); i++)
                if (e == encodings[i].encoding)
                        return rb_str_new2(encodings[i].id);

        return Qnil;
}

static VALUE
frame_field_get_stringlist(union id3_field *field)
{
        unsigned int n = id3_field_getnstrings(field);
        VALUE ary = rb_ary_new2(n);

        for (unsigned int i = 0; i < n; i++)
                rb_ary_push(ary, str_new_ucs4(id3_field_getstrings(field, i)));

        return ary;
}

static VALUE
frame_field_get(VALUE self, VALUE n)
{
        union id3_field *field = id3_frame_field(value2frame(self), FIX2UINT(n));

        if (field == NULL)
                return Qnil;

        switch (id3_field_type(field)) {
        case ID3_FIELD_TYPE_TEXTENCODING:
                return frame_field_get_textencoding(field);
        case ID3_FIELD_TYPE_LATIN1:
                return rb_str_new2((char *)id3_field_getlatin1(field));
        case ID3_FIELD_TYPE_LATIN1FULL:
                return rb_str_new2((char *)id3_field_getfulllatin1(field));
        case ID3_FIELD_TYPE_STRING:
                return str_new_ucs4(id3_field_getstring(field));
        case ID3_FIELD_TYPE_STRINGFULL:
                return str_new_ucs4(id3_field_getfullstring(field));
        case ID3_FIELD_TYPE_STRINGLIST:
                return frame_field_get_stringlist(field);
        case ID3_FIELD_TYPE_LANGUAGE:
        case ID3_FIELD_TYPE_DATE:
                return rb_str_new2(field->immediate.value);
        case ID3_FIELD_TYPE_FRAMEID:
                return rb_str_new2(id3_field_getframeid(field));
        case ID3_FIELD_TYPE_INT8:
        case ID3_FIELD_TYPE_INT16:
        case ID3_FIELD_TYPE_INT24:
                return INT2FIX(id3_field_getint(field));
        case ID3_FIELD_TYPE_INT32:
                return INT2NUM(id3_field_getint(field));
        case ID3_FIELD_TYPE_BINARYDATA: {
                id3_length_t len;
                id3_byte_t const *bytes = id3_field_getbinarydata(field, &len);

                return rb_str_new((char *)bytes, len);
        }
        case ID3_FIELD_TYPE_INT32PLUS:
        case ID3_FIELD_TYPE_LATIN1LIST:
        default:
                rb_notimplement();
        }

        return Qnil;
}

static void
ucs4_field_set(union id3_field *field, int (*f)(union id3_field *, id3_ucs4_t const *), VALUE str)
{
        id3_ucs4_t *ucs4 = id3_utf8_ucs4duplicate((id3_utf8_t *)StringValuePtr(str));
        f(field, ucs4);
        free(ucs4);
}

static void
frame_field_set_textencoding(union id3_field *field, VALUE value)
{
        char *str = StringValuePtr(value);

        for (unsigned int i = 0; i < sizeof(encodings) / sizeof(encodings[0]); i++)
                if (strcmp(str, encodings[i].id) == 0) {
                        id3_field_settextencoding(field, encodings[i].encoding);
                        return;
                }
}

static void
frame_field_set_stringlist(union id3_field *field, VALUE value)
{
        Check_Type(value, T_ARRAY);

        long n = RARRAY(value)->len;
        id3_ucs4_t *ucs4s[n];

        for (long i = 0; i < n; i++) {
                VALUE entry = rb_ary_entry(value, i);
                ucs4s[i] = id3_utf8_ucs4duplicate((id3_utf8_t *)StringValuePtr(entry));
        }

        id3_field_setstrings(field, n, ucs4s);

        for (long i = 0; i < n; i++)
                free(ucs4s[i]);
}

static VALUE
frame_field_set(VALUE self, VALUE n, VALUE value)
{
        union id3_field *field = id3_frame_field(value2frame(self), FIX2UINT(n));

        switch (id3_field_type(field)) {
        case ID3_FIELD_TYPE_TEXTENCODING:
                frame_field_set_textencoding(field, value);
                break;
        case ID3_FIELD_TYPE_LATIN1:
                id3_field_setlatin1(field, (id3_latin1_t *)StringValuePtr(value));
                break;
        case ID3_FIELD_TYPE_LATIN1FULL:
                id3_field_setfulllatin1(field, (id3_latin1_t *)StringValuePtr(value));
                break;
        case ID3_FIELD_TYPE_STRING:
                ucs4_field_set(field, id3_field_setstring, value);
                break;
        case ID3_FIELD_TYPE_STRINGFULL:
                ucs4_field_set(field, id3_field_setfullstring, value);
                break;
        case ID3_FIELD_TYPE_STRINGLIST:
                frame_field_set_stringlist(field, value);
                break;
        case ID3_FIELD_TYPE_LANGUAGE:
                id3_field_setlanguage(field, StringValuePtr(value));
                break;
        case ID3_FIELD_TYPE_DATE:
                memcpy(field->immediate.value, StringValuePtr(value), sizeof(field->immediate.value));
                break;
        case ID3_FIELD_TYPE_FRAMEID:
                id3_field_setframeid(field, StringValuePtr(value));
                break;
        case ID3_FIELD_TYPE_INT8:
        case ID3_FIELD_TYPE_INT16:
        case ID3_FIELD_TYPE_INT24:
                id3_field_setint(field, FIX2INT(value));
                break;
        case ID3_FIELD_TYPE_INT32:
                id3_field_setint(field, NUM2INT(value));
                break;
        case ID3_FIELD_TYPE_BINARYDATA:
                StringValue(value);
                id3_field_setbinarydata(field, (id3_byte_t *)RSTRING(value)->ptr, RSTRING(value)->len);
                break;
        case ID3_FIELD_TYPE_INT32PLUS:
        case ID3_FIELD_TYPE_LATIN1LIST:
        default:
                rb_notimplement();
        }

        return self;
}

static struct id3_tag *
value2tag(VALUE self)
{
        struct id3_tag *tag;

        Data_Get_Struct(self, struct id3_tag, tag);

        return tag;
}

static VALUE
tag_allocate(VALUE klass)
{
        return Data_Wrap_Struct(klass, NULL, id3_tag_delete, NULL);
}

static VALUE
tag_initialize(VALUE self)
{
        DATA_PTR(self) = id3_tag_new();
        
        return self;
}

static VALUE
tag_version(VALUE self)
{
        return UINT2NUM(id3_tag_version(value2tag(self)));
}

static VALUE
tag_options_get(VALUE self)
{
        return INT2FIX(id3_tag_options(value2tag(self), 0, 0));
}

static VALUE
tag_options_set(VALUE self, VALUE mask, VALUE options)
{
        return INT2FIX(id3_tag_options(value2tag(self), FIX2INT(mask), FIX2INT(options)));
}

static VALUE
tag_length_set(VALUE self, VALUE length)
{
        id3_tag_setlength(value2tag(self), NUM2ULONG(length));

        return Qnil;
}

static VALUE
tag_clear_frames(VALUE self)
{
        id3_tag_clearframes(value2tag(self));

        return Qnil;
}

static VALUE
tag_frame_attach(VALUE self, VALUE frame)
{
        return INT2FIX(id3_tag_attachframe(value2tag(self), value2frame(frame)));
}

static VALUE
tag_frame_detach(VALUE self, VALUE frame)
{
        return INT2FIX(id3_tag_detachframe(value2tag(self), value2frame(frame)));
}

static VALUE
tag_frame_find(VALUE self, VALUE id, VALUE index)
{

        struct id3_frame *frame = id3_tag_findframe(value2tag(self), id == Qnil ? NULL : StringValuePtr(id), FIX2INT(index));

        if (frame == NULL)
                return Qnil;

        return Data_Wrap_Struct(cID3Frame, NULL, id3_frame_delete, frame);
}

static VALUE
s_tag_query(VALUE self, VALUE data)
{
        self = self;

        StringValue(data);

        return ULONG2NUM(id3_tag_query((id3_byte_t *)RSTRING(data)->ptr, RSTRING(data)->len));
}

static VALUE
s_tag_parse(VALUE self, VALUE data)
{
        self = self;

        StringValue(data);

        return Data_Wrap_Struct(cID3Tag,
                                NULL,
                                id3_tag_delete,
                                id3_tag_parse((id3_byte_t *)RSTRING(data)->ptr, RSTRING(data)->len));
}

static VALUE
tag_render(VALUE self)
{
        id3_length_t tag_length = id3_tag_render(value2tag(self), NULL);
        VALUE rendered_tag = rb_str_buf_new(tag_length);
        tag_length = id3_tag_render(value2tag(self), (id3_byte_t *)RSTRING(rendered_tag)->ptr);
        RSTRING(rendered_tag)->len = tag_length;

        return rendered_tag;
}

extern void id3_tag_addref(struct id3_tag *tag);
extern void id3_tag_delref(struct id3_tag *tag);

static VALUE
tag_ref(VALUE self)
{
        id3_tag_addref(value2tag(self));
        return Qnil;
}

static VALUE
tag_unref(VALUE self)
{
        id3_tag_delref(value2tag(self));
        return Qnil;
}

static VALUE
tag_extended_flags(VALUE self)
{
        return INT2FIX(value2tag(self)->extendedflags);
}

static VALUE
tag_extended_flags_set(VALUE self, VALUE extended_flags)
{
        return INT2FIX(value2tag(self)->extendedflags = FIX2INT(extended_flags));
}

static VALUE
tag_flags(VALUE self)
{
        return INT2FIX(value2tag(self)->flags);
}

static VALUE
tag_flags_set(VALUE self, VALUE flags)
{
        return INT2FIX(value2tag(self)->flags = FIX2INT(flags));
}

void Init_id3tag(void);

void
Init_id3tag(void)
{
        VALUE mID3 = rb_define_module("ID3");

        cID3Frame = rb_define_class_under(mID3, "Frame", rb_cData);
        rb_define_alloc_func(cID3Frame, frame_allocate);
        rb_define_private_method(cID3Frame, "initialize", frame_initialize, 1);
        rb_define_method(cID3Frame, "field_type", frame_field_type, 1);
        rb_define_method(cID3Frame, "[]", frame_field_get, 1);
        rb_define_method(cID3Frame, "[]=", frame_field_set, 2);
        rb_define_method(cID3Frame, "type", frame_type, 0);
        rb_define_method(cID3Frame, "description", frame_description, 0);

        cID3Tag = rb_define_class_under(mID3, "Tag", rb_cData);
        rb_define_alloc_func(cID3Tag, tag_allocate);
        rb_define_private_method(cID3Tag, "initialize", tag_initialize, 0);
        rb_define_method(cID3Tag, "version", tag_version, 0);
        rb_define_method(cID3Tag, "options", tag_options_get, 0);
        rb_define_method(cID3Tag, "set_options", tag_options_set, 2);
        rb_define_method(cID3Tag, "length=", tag_length_set, 1);
        rb_define_method(cID3Tag, "clear_frames", tag_clear_frames, 0);
        rb_define_method(cID3Tag, "frame_attach", tag_frame_attach, 1);
        rb_define_method(cID3Tag, "frame_detach", tag_frame_detach, 1);
        rb_define_method(cID3Tag, "frame_find", tag_frame_find, 2);
        rb_define_method(cID3Tag, "render", tag_render, 0);
        rb_define_method(cID3Tag, "ref", tag_ref, 0);
        rb_define_method(cID3Tag, "unref", tag_unref, 0);
        rb_define_method(cID3Tag, "extended_flags", tag_extended_flags, 0);
        rb_define_method(cID3Tag, "extended_flags=", tag_extended_flags_set, 1);
        rb_define_method(cID3Tag, "flags", tag_flags, 0);
        rb_define_method(cID3Tag, "flags=", tag_flags_set, 1);
        rb_define_singleton_method(cID3Tag, "tag_query", s_tag_query, 1);
        rb_define_singleton_method(cID3Tag, "tag_parse", s_tag_parse, 1);

        VALUE mID3TagOption = rb_define_module_under(cID3Tag, "Option");
        rb_define_const(mID3TagOption, "Unsynchronize", INT2FIX(ID3_TAG_OPTION_UNSYNCHRONISATION));
        rb_define_const(mID3TagOption, "Compress", INT2FIX(ID3_TAG_OPTION_COMPRESSION));
        rb_define_const(mID3TagOption, "CRC", INT2FIX(ID3_TAG_OPTION_CRC));
        rb_define_const(mID3TagOption, "Append", INT2FIX(ID3_TAG_OPTION_APPENDEDTAG));
        rb_define_const(mID3TagOption, "FileAltered", INT2FIX(ID3_TAG_OPTION_FILEALTERED));
        rb_define_const(mID3TagOption, "ID3v1", INT2FIX(ID3_TAG_OPTION_ID3V1));
}
