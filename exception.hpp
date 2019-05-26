#pragma once
namespace UOS{
	
	class exception{
		
	protected:
		
		const char * msg;
		
		exception(const char*);
	public:
		exception(void);
	
		virtual ~exception(void);
		virtual const char* what(void) const;
		exception& operator=(const exception& sor);
	
	};

	class out_of_range : public exception{
	public:
		out_of_range(void);

	};
}