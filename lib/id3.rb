# contents: ID3 handling based on libid3tag.
#
# Copyright © 2007 Nikolai Weibull <now@bitwi.se>

require 'id3tag'

class ID3::Tag; end

module ID3::Tag::Flags
  Unsynchronisation = 1 << 7
  ExtendedHeader = 1 << 6
  ExperimentalIndicator = 1 << 5
  FooterPresent = 1 << 4
end

module ID3::Tag::Flags::Extended
  TagIsAnUpdate = 1 << 6
  CRCDataPresent = 1 << 5
  TagRestrictions = 1 << 4
end

class ID3::Tag
  def [](id, index = 0)
    case id
    when Integer
      index = id
      id = nil
    end
    frame_find(id, index)
  end

  def each
    i = 0
    while frame = self[i]
      yield frame
      i += 1
    end
    self
  end

  [ [:unsynchronized, Flags::Unsynchronisation],
    [:has_extended_header, Flags::ExtendedHeader],
    [:experimental, Flags::ExterimentalIndicator],
    [:has_footer, Flags::FooterPresent] ].each do |name, bit|
    define_method(:"#{name}?") do
      (flags & bit) != 0
    end

    define_method(:"#{name}=") do |value|
      flags &= ~bit
      flags |= bit if vale
      value
    end
  end

  [ [:is_an_update, Flags::Extended::TagIsAnUpdate] ].each do |name, bit|
    define_method(:"#{name}?") do
      (extended_flags & bit) != 0
    end

    define_method(:"#{name}=") do |value|
      extended_flags &= ~bit
      extended_flags |= bit if vale
      value
    end
  end

  [ [:render_v1, Option::ID3v1],
    [:append, Option::Append] ].each do |name, option|
    define_method(:"#{name}?") do
      (options & option) != 0
    end

    define_method(:"#{name}=") do |value|
      set_options(ID3::Tag::Option::ID3v1, value ? ~0 : 0)
      value
    end
  end
end

class ID3::Frame
  def each
    i = 0
    while field = self[i]
      yield field
      i += 1
    end
    self
  end
end

class ID3::File
  def self.open(path, mode = 'r')
    file = self.new(path, mode)
    return file unless block_given?
    yield file
    file.close
    nil
  end

  # TODO: Move everything that has to do with tags to find_tags.
  def initialize(path, mode = 'r')
    @io = File.open(path, mode)
    @flags = 0
    @has_v1 = false
    @primary = ID3::Tag.new
    @primary.ref
    find_tags
    create_tag if @tags.empty?
    @primary.render_v1 = @has_v1
  end

  def close
    @io.close
    @primary.unref
    @tags.each{ |tag| tag.tag.unref if tag.tag }
  end

  def tag
    @primary
  end

  def update
    render_v1 = @primary.render_v1?
    v1 = render_v1 ? primary.render : nil
    @primary.render_v1 = false
    v2 = @primary.render
    write_v2(v2)
    write_v1(v1)
    @io.pos = 0
    @primary.render_v1 = render_v1
  end

private

  class FileTag
    def initialize(tag, location, length)
      @tag, @location, @length = tag, location, length
    end

    def <=>(other)
      @location - other.location
    end

    attr_reader :tag, :location, :length
  end

  def save_excursion
    saved_pos = @io.pos
    result = yield
    @io.pos = saved_pos
    result
  end

  def write_v2(data)
    return if not (@tags.length > 0 and not @has_v1) and not (@tags.length > 1 and @has_v1)
    case
    when @tags[0].length == data.length
      @io.pos = @tags[0].location
      @io.write data
      @io.flush
    else
      @io.seek(0, IO::SEEK_END)
      length_before = @io.pos
      @io.pos = @tags[0].location + @tags[0].length
      remainder = @io.read
      @io.pos = @tags[0].location
      @io.write data
      @io.write remainder
      @io.flush
      @io.truncate @io.pos if @io.pos < length_before
    end
  end

  def write_v1(data)
    if data
      @io.seek(@has_v1 ? -128 : 0, IO::SEEK_END)
      location = @io.pos
      @io.write data
      @io.flush
      if not @has_v1
        add_filetag(FileTag.new(nil, location, 128))
        @has_v1 = true
      end
    else
      if @has_v1
        @io.seek(0, IO::SEEK_END)
        length = @io.pos
        return if length < 0 || length < 128
        @io.truncate(length - 128)
        @tags.pop
        @has_v1 = false
      end
    end
  end

  def create_tag
    tag = ID3::Tag.new
    file_tag = FileTag.new(tag, 0, 0)
    add_filetag(file_tag)
    update_primary(tag)
    tag.ref
  end

  def find_tags
    @tags = []
    save_excursion do
      find_v1_tag
      find_v2_tag
    end
    if (@tags.length > 0 and not @has_v1) or
       (@tags.length > 1 and     @has_v1)
      # XXX: OK, we have a v2 tag.
      if @tags[0].location == 0
        @primary.length = @tags[0].length
      else
        @primary.append = true
      end
    end
  end

  def version_major(version)
    (version >> 8) & 0xff
  end

  def find_v1_tag
    @io.seek(-128, IO::SEEK_END)
    size = query_tag
    return unless size > 0
    tag = add_tag size
    if version_major(tag.version) == 1
      @has_v1 = true
    end
  end

  def find_v2_tag
    @io.pos = 0
    size = query_tag
    return unless size > 0
    tag = add_tag(size)
    while frame = tag["SEEK"]
      seek = frame[0]
      break if seek < 0
      @io.seek(seek, IO::SEEK_CUR)
      size = query_tag
      break unless size > 0
      tag = add_tag(size)
    end
    @io.seek((@has_v1 ? -128 : 0) + -QuerySize, IO::SEEK_END)
    size = query_tag
    if size > 0
      @io.seek(size , IO::SEEK_CUR)
      size = query_tag
      add_tag(size) if size > 0
    end
  end

  QuerySize = 10

  def query_tag
    save_excursion{ ID3::Tag.tag_query(@io.read(QuerySize)) }
  end

  def add_tag(length)
    location = @io.pos
    if file_tag = find_duplicate(location, length)
      return file_tag.tag
    end
    return nil if find_overlap(location, length)
    tag = read_tag(length)
    add_filetag(FileTag.new(tag, location, length))
    update_primary(tag)
    tag.ref
    tag
  end

  def add_filetag(tag)
    @tags << tag
    @tags = @tags.sort
  end

  def update_primary(tag)
    @primary.clear_frames unless tag.is_an_update?
    tag.each{ |frame| @primary.frame_attach frame }
  end

  def read_tag(length)
    ID3::Tag.tag_parse(@io.read(length))
  end

  def find_among_previous_tags(location, length)
    begin1 = location
    end1 = begin1 + length
    @tags.find do |tag|
      begin2 = tag.location
      end2 = begin2 + tag.length
      yield begin1, end1, begin2, end2
    end
  end

  def find_duplicate(location, length)
    find_among_previous_tags(location, length){ |begin1, end1, begin2, end2| begin1 == begin2 and end1 == end2 }
  end

  def find_overlap(location, length)
    find_among_previous_tags(location, length){ |begin1, end1, begin2, end2| begin1 < end2 and end1 > begin2 }
  end
end

if __FILE__ == $0
  ID3::File.open('01-har_du_problem-radio_edit.mp3', 'r+') do |file|
  #  file.tag.frame_find('TIT2', 0)[1] = ['Har du “problem”? (Album Mix)']
    file.tag.each do |frame|
      puts "#{frame.type} (#{frame.description}):"
      frame.each do |field|
        puts "  #{field}"
      end
    end
    file.update
  end
end
