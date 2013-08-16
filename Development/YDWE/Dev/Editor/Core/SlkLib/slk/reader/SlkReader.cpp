
#include <slk/port/config.h>
#include <slk/utility/sequence.h>
#include <slk/utility/convert.h>
#include <slk/reader/SlkReader.hpp>
#include <slk/reader/CommonReader.hpp>

namespace slk
{
	namespace
	{
#pragma pack(push, 1)
		struct SLK_HEADER
		{
			char szMagic[11]; //"ID;PWXL;N;E"

			bool IsValid() const
			{
				return 0 == memcmp("ID;PWXL;N;E", szMagic, sizeof(szMagic)/sizeof(szMagic[0]));
			}
		};
#pragma pack(pop)


		class SlkDataArray : public std::vector<std::vector<std::string>>
		{
		public:
			typedef std::vector<std::vector<std::string>> _Mybase;

			SlkDataArray(buffer_reader& reader)
			{
				read(reader);
			}

			void convert(SlkTable& table)
			{
				for (size_t y = 1; y < max_y_; ++y)
				{
					if (id_[y].vaild())
					{
						SlkSingle& obj = table[id_[y]];
						for (size_t x = 1; x < max_x_; ++x)
						{
							if ((!(*this)[x][y].empty()) && (!tag_[x].empty()))
							{
								obj[tag_[x]] = (*this)[x][y];
							}
						};
					}
				}
			}

		private:
			void assign(size_t max_x, size_t max_y)
			{
				max_x_ = max_x;
				max_y_ = max_y;
				tag_.resize(max_x_);
				id_.resize(max_y_);
				_Mybase::assign(max_x_, std::vector<std::string>(max_y_));
			}

			void set_x(size_t x)
			{
				cur_x_ = x;
			}

			void set_y(size_t y)
			{
				cur_y_ = y;
			}

			void set_k(boost::string_ref&& val)
			{
				if (cur_y_ == 1)
				{
					if (val.size() >= 2 && val[0] == '"' && val[val.size()-1] == '"')
					{
						val.remove_prefix(1);
						val.remove_suffix(1);
					}

					tag_[cur_x_-1] = val.to_string();
				}
				else if (cur_x_ == 1)
				{
					if (val.size() >= 2 && val[0] == '"' && val[val.size()-1] == '"')
					{
						val.remove_prefix(1);
						val.remove_suffix(1);
					}

					id_[cur_y_-1] = object_id(val);
				}
				else
				{
					(*this)[cur_x_-1][cur_y_-1] = val.to_string();
				}
			}

			template <typename SequenceT, typename ResultT>
			void split_callback(SequenceT const& Input, ResultT Callback)
			{
				typedef typename SequenceT::const_iterator ForwardIteratorT;
				ForwardIteratorT InBegin = Input.begin();
				ForwardIteratorT InEnd = Input.end();
				ForwardIteratorT InCur = Input.begin();
				int stat = 0;

				for (ForwardIteratorT It = InBegin; It != InEnd; )
				{
					for (; It != InEnd; ++It)
					{
						if (stat == 0)
						{
							if (*It == '"')
							{
								stat = 1;
							}
							else if (*It == ';')
							{
								Callback(InCur, It);
								break;
							}
						}
						else if (stat == 1)
						{
							if (*It == '"')
							{
								stat = 0;
							}
							else if (*It == '\\')
							{
								stat = 2;
							}
						}
						else if (stat == 2)
						{
							stat = 1;
						}
					}

					if (It != InEnd)
					{
						InCur = ++It;
					}
				}
				if (InCur != InEnd)
				{
					Callback(InCur, InEnd);
				}
			}

			void read_line_b(boost::string_ref& line)
			{
				size_t x = 0, y = 0;

				split_callback(line,
					[&](boost::string_ref::const_iterator const& beg, boost::string_ref::const_iterator const& end)
				{
					switch (*beg)
					{
					case 'X':
						{
							x = Str2UInt(trim_copy<boost::string_ref>(beg+1, end));
						}
						break;
					case 'Y':
						{
							y = Str2UInt(trim_copy<boost::string_ref>(beg+1, end));
						}
						break;
					default:
						break;
					}
				});

				if ((x == 0) || (y == 0))
				{
					throw ydwe::exception("slk data corrupted.") ;
				}

				this->assign(x, y);
			}

			void read_line_c(boost::string_ref& line)
			{
				split_callback(line,
					[&](boost::string_ref::const_iterator const& beg, boost::string_ref::const_iterator const& end)
				{
					switch (*beg)
					{
					case 'X':
						{
							this->set_x(Str2UInt(trim_copy<boost::string_ref>(beg+1, end)));
						}
						break;
					case 'Y':
						{
							this->set_y(Str2UInt(trim_copy<boost::string_ref>(beg+1, end)));
						}
						break;
					case 'K':
						{
							this->set_k(trim_copy<boost::string_ref>(beg+1, end));
						}
						break;
					default:
						break;
					}
				});
			}

			void read_line_f(boost::string_ref& line)
			{
				split_callback(line,
					[&](boost::string_ref::const_iterator const& beg, boost::string_ref::const_iterator const& end)
				{
					switch (*beg)
					{
					case 'X':
						{
							this->set_x(Str2UInt(trim_copy<boost::string_ref>(beg+1, end)));
						}
						break;
					case 'Y':
						{
							this->set_y(Str2UInt(trim_copy<boost::string_ref>(beg+1, end)));
						}
						break;
					default:
						break;
					}
				});
			}

			uint8_t read_type(boost::string_ref& line)
			{
				auto It = find_begin(line, char_equal(';'));

				if (It == line.end())
				{
					return 0;
				}

				boost::string_ref key = trim_copy<boost::string_ref>(line.begin(), It);

				line.remove_prefix(It - line.begin() + 1);
				return key.front();
			}

			void read(buffer_reader& reader)
			{
				bool is_found_b = false;
				reader::utility::each_line(reader, [&](boost::string_ref& line)
				{
					uint8_t type = read_type(line);
					if (!is_found_b)
					{
						if (type == 'B')
						{
							read_line_b(line);
							is_found_b = true;
						}
					}
					else
					{
						if (type == 'C')
						{
							read_line_c(line);
						}
						else if (type == 'F')
						{
							read_line_f(line);
						}
					}
				});
			}

		private:
			size_t max_x_;
			size_t max_y_;
			size_t cur_x_;
			size_t cur_y_;
			std::vector<object_id>    id_;
			std::vector<std::string> tag_;
		};

	}

	void SlkReader::Read(buffer_reader& reader, SlkTable& table)
	{
		if (!reader.read_ptr<SLK_HEADER>()->IsValid())
		{
			throw ydwe::exception("slk data corrupted.") ;
		}

		SlkDataArray(reader).convert(table);
	}
}
