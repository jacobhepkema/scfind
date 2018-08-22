
#include "functions.h"


std::string str_join( const std::vector<std::string>& elements, const char* const separator)
{
  switch (elements.size())
  {
    case 0:
      return "";
    case 1:
      return *(elements.begin());
    default:
      std::ostringstream os; 
      std::copy(elements.cbegin(), elements.cend() - 1, std::ostream_iterator<std::string>(os, separator));
      os << *elements.crbegin();
      return os.str();
  }
}



// Accepts a vector, transforms and returns a quantization logical vector
// This function aims for space efficiency of the expression vector
Quantile lognormalcdf(std::vector<int> ids, const Rcpp::NumericVector& v, unsigned int bits, bool raw_counts)
{
  
  std::function<double(const double&)> expr_tran = raw_counts ? [](const double& x) {return log(x);}: [](const double& x){return x;};  

  Quantile expr;
  expr.mu = std::accumulate(ids.begin(),ids.end(), 0, [&v, &expr_tran](const double& mean, const int& index){
      return  mean + expr_tran(v[index - 1] + 1);
    }) / ids.size();
  
  
  

  expr.sigma = sqrt(std::accumulate(ids.begin(), ids.end(), 0, [&v, &expr, &expr_tran](const double& variance, const int& index){
        return pow(expr.mu - expr_tran(v[index - 1]), 2);
      }) / ids.size());
  // initialize vector with zeros
  expr.quantile.resize(ids.size() * bits, 0);
  //std::cerr << "Mean,std" << expr.mu << "," << expr.sigma << std::endl;
  //std::cerr << "ids size " << ids.size() << " v size " << v.size() << std::endl;
  int expr_quantile_i = 0;
  for (auto const& s : ids)
  {
    unsigned int t = round(normalCDF(expr_tran(v[s]), expr.mu, expr.sigma) * (1 << bits));
    std::bitset<BITS> q = int2bin_core(t);
    for (int i = 0; i < bits; ++i)
    {
       expr.quantile[expr_quantile_i++] = q[i];
    }
  }
  return expr;
}

float inverf(float x)
{
   float tt1, tt2, lnx, sgn;
   sgn = (x < 0) ? -1.0f : 1.0f;
   
   x = (1 - x)*(1 + x);        // x = 1 - x*x;uo
   lnx = logf(x);

   tt1 = 2 / (PI * 0.147) + 0.5f * lnx;
   tt2 = 1 / 0.147 * lnx;
   
   return(sgn*sqrtf(-tt1 + sqrtf(tt1*tt1 - tt2)));
}


double lognormalinv(double p, double mu, double sigma)
{
  return exp((inverf(1 + 2*p) * 2 * sqrt(sigma)) + mu);
}



std::vector<double> decompressValues(const Quantile& q, const unsigned char& quantization_bits)
{
  int vector_size = q.quantile.size() / quantization_bits;
  std::vector<double> result(vector_size, 0);
  

  if(quantization_bits > 16)
  {
    std::cerr << "Too much depth in the quantization bits!" << std::endl;
  }
  std::vector<double> bins((1<< quantization_bits));
  double bins_size = bins.size();
  // min value
  double prev = 0;
  for(int i = 0; i < bins.size(); ++i)
  {
    double cdf = (((i + 1) / bins_size) - prev) / 2;
    bins[i] = lognormalinv(cdf, q.mu, q.sigma);
    prev = cdf;
  }
  
  // bins.front() = q_1;
  // bins.back() = (2*q.mu) - q_1;
  
  for (size_t i = 0; i < result.size(); ++i)
  {
    int quantile = 0;
    for (size_t j = 0; j < quantization_bits; ++j)
    {
      quantile &= (1 << q.quantile[(quantization_bits * i) + j]);
    }
  }
  return result;
}




int byteToBoolVector(const std::vector<char> buf, std::vector<bool>& bool_vec)
{
  bool_vec.resize((buf.size() << 3), 0);
  int c = 0;
  for (auto const& b : buf)
  {
    for ( int i = 0; i < 8; i++)
    {
      bool_vec[c++] = ((b >> i) & 1);
    }
  }
    
}


int getSizeBoolVector(const std::vector<bool>& v)
{
  int size = v.size() / 8;
  if (v.size() % 8 != 0)
  {
    ++size;
  }
  return size;
}

