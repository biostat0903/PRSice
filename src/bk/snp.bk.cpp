#include "snp.hpp"


SNP::SNP(const std::string rs_id, const std::string chr, const int loc,
		const std::string ref_allele, const std::string alt_allele,
		const double statistic, const double se, const double p_value)
	: m_ref(ref_allele), m_alt(alt_allele),
		m_rs(rs_id), m_chr(chr), m_loc(loc),  m_stat(statistic),
		m_se(se), m_p_value(p_value)
{
    m_bit_size = sizeof(long_type)*CHAR_BIT;
    m_flipped = false;
    m_clumped = false;
}

std::vector<size_t> SNP::sort_by_p(const boost::ptr_vector<SNP> &input)
{
    std::vector<size_t> idx(input.size());
    std::iota(idx.begin(), idx.end(),0);
    std::sort(idx.begin(), idx.end(), [&input](size_t i1, size_t i2)
    {
    	// plink do it with respect to the location instead of statistic
        if(input[i1].m_p_value==input[i2].m_p_value)
        {
        	if(input[i1].m_chr.compare(input[i2].m_chr)==0)
        	{
        		if(input[i1].m_loc == input[i2].m_loc)
        		{
        			if(fabs(input[i1].m_stat)==fabs(input[i2].m_stat))
        			{
        				return input[i1].m_se < input[i2].m_se;
        			}
        			else return fabs(input[i1].m_stat) > fabs(input[2].m_stat);
        		}
        		else return input[i1].m_loc < input[i2].m_loc;
        	}
			else return input[i1].m_chr.compare(input[i2].m_chr)<0;
        }
        else return input[i1].m_p_value < input[i2].m_p_value;
    });
    return idx;
}

SNP::SNP()
{
    m_loc=-1; // default is -1 to indicate that it is not provided
    m_stat=0.0;
    m_se=0.0;
    m_p_value=0.0;
    m_flipped = false;
    m_clumped = false;
    m_bit_size = sizeof(long_type)*CHAR_BIT;
}

SNP::~SNP(){}

size_t SNP::index_check(const std::string &c_in)
{
    int temp = atoi(c_in.c_str());
    if(temp < 0) throw std::runtime_error("Index of column cannot be less than 0");
    return temp;
}

size_t SNP::index_check(const std::string &c_in, const std::vector<std::string> &c_header, const std::string &typeOfError)
{
    for(size_t i = 0; i < c_header.size(); ++i)
    {
        if(c_in.compare(c_header[i])==0)
        {
            return i;
        }
    }
    std::string error_message = typeOfError+": No "+c_in+" colume in input data";
    if(typeOfError.compare("ERROR")==0) throw std::runtime_error(error_message);
    else fprintf(stderr, "%s\n", error_message.c_str());
    return -1; //Cannot find the index
}

std::vector<int> SNP::get_index(const Commander &c_commander, const std::string &c_input)
{
    // This function should return the index in the following order
    // CHR, A1, A2, STAT, SNP, BP, SE, P
    // Absent field = -1
    // Here we will also check if the field are actually within the file
    std::vector<int> result(9,-1);
    if(c_commander.index())
    {
        //Index was provided, check if the index is correct, then return the vector
        result[+SNP_Index::CHR] = index_check(c_commander.chr());
        result[+SNP_Index::REF] = index_check(c_commander.ref());
        result[+SNP_Index::ALT] = index_check(c_commander.alt());
        result[+SNP_Index::STAT] = index_check(c_commander.statistic());
        result[+SNP_Index::RS] = index_check(c_commander.snp());
        result[+SNP_Index::BP] = index_check(c_commander.bp());
        result[+SNP_Index::SE] = index_check(c_commander.se());
        result[+SNP_Index::P] = index_check(c_commander.p());
    }
    else
    {
        std::ifstream in;
        in.open(c_input.c_str());
        if(!in.is_open())
        {
            std::string error_message = "Cannot open file: "+ c_input;
            throw std::runtime_error(error_message);
        }
        std::string header_line;
        std::getline(in, header_line);
        in.close();
        if(header_line.empty())
        {
            std::string error_message = "Empty header line for "+c_input;
            throw std::runtime_error(error_message);
        }
        std::vector<std::string> header = misc::split(header_line);
        result[+SNP_Index::CHR] = index_check(c_commander.chr(), header, "WARNING");
        result[+SNP_Index::REF] = index_check(c_commander.ref(), header, "ERROR");
        result[+SNP_Index::ALT] = index_check(c_commander.alt(), header, "WARNING");
        result[+SNP_Index::STAT] = index_check(c_commander.statistic(), header, "ERROR");
        result[+SNP_Index::RS] = index_check(c_commander.snp(), header, "ERROR");
        result[+SNP_Index::BP] = index_check(c_commander.bp(), header, "WARNING");
        result[+SNP_Index::SE] = index_check(c_commander.se(), header, "WARNING");
        result[+SNP_Index::P] = index_check(c_commander.p(), header, "ERROR");
        sort( header.begin(), header.end() );
        size_t before = header.size();
        header.erase( unique( header.begin(), header.end() ), header.end() );
        size_t after= header.size();
        if(before!=after)
        {
            fprintf(stderr, "WARNING: Header contain duplicated elements\n");
            fprintf(stderr, "         Only the first occurrence is used\n");
            fprintf(stderr, "         Please do check your input file\n");
        }
    }
    int max_index = -1;
    for(size_t i = 0; i < 9; ++i) max_index = (result[i]> max_index)? result[i]:max_index; //get the maximum index
    result[+SNP_Index::MAX] = max_index;
    if(c_commander.index())
    {
        std::ifstream in;
        in.open(c_input.c_str());
        std::string line;
        std::getline(in, line);
        std::vector<std::string> col = misc::split(misc::trimmed(line));
        if(col.size() < max_index)
        {
            throw std::runtime_error("ERROR: Number of column in file less than the specified index!");
        }
    }
    return result;
}


void SNP::clump(boost::ptr_vector<SNP> &snp_list)
{
	for(auto &&target : m_clump_target){
		if(!snp_list[target].clumped())
		{
			int sum_total = 0;
			for(size_t i_flag = 0; i_flag < m_flags.size(); ++i_flag)
			{
				//TODO: ERROR IS HERE
				// if there is any overlap this should set the snp_list to the new flag
				snp_list[target].m_flags[i_flag] = snp_list[target].m_flags[i_flag] ^
						(m_flags[i_flag] & snp_list[target].m_flags[i_flag]);
				sum_total+=snp_list[target].m_flags[i_flag];
			}
			if(sum_total==0)  snp_list[target].set_clumped();
		}
	}
	m_clumped=true; // protect from other SNPs tempering its flags
}

void SNP::proxy_clump(boost::ptr_vector<SNP> &snp_list, double r2_threshold)
{
    for(size_t i_target = 0; i_target < m_clump_target.size(); ++i_target)
    {
        if(!snp_list[m_clump_target[i_target]].clumped())
        {
            snp_list[m_clump_target[i_target]].set_clumped();
            if(m_clump_r2[i_target] >= r2_threshold)
            {
                for(size_t j = 0; j < m_flags.size(); ++j)  m_flags[j] |= snp_list[m_clump_target[i_target]].m_flags[j];
            }
        }
    }
	m_clumped=true; // protect from other SNPs tempering its flags
}


bool SNP::check_loc(const std::string &chr, const int loc, const std::string &ref_allele,
                    const std::string &alt_allele)
{
    if(chr.compare(m_chr)!=0) return false;
    if(loc!= m_loc) return false;
    //Check if allele is the same
    if(ref_allele.compare(m_ref)!=0 && alt_allele.compare(m_ref)!=0 &&
            ref_allele.compare(complement(m_ref))!=0 &&
            alt_allele.compare(complement(m_ref))!=0 ) return false; // not possible even after flipping
    if(m_alt.empty())
    {
        // can only use the reference allele to do things, more dangerous
        if((ref_allele.compare(m_ref)!=0 && alt_allele.compare(m_ref)==0) ||
                (ref_allele.compare(complement(m_ref))!=0 && alt_allele.compare(complement(m_ref))==0))
        {
            m_alt = ref_allele;
            m_ref = alt_allele;
            m_flipped=true;
        }
    }
    else
    {
        // can use both
        if((ref_allele.compare(m_alt)==0 && alt_allele.compare(m_ref)==0) ||
                (ref_allele.compare(complement(m_alt))==0 &&
                 alt_allele.compare(complement(m_ref))==0)	)
        {
            // need to flip
        		m_alt = ref_allele;
            m_ref = alt_allele;
            m_flipped=true;
        }
    }
    return true;
}

