// Jubatus: Online machine learning framework for distributed environment
// Copyright (C) 2011 Preferred Infrastracture and Nippon Telegraph and Telephone Corporation.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#include "recommender_serv.hpp"

#include <pficommon/concurrent/lock.h>
#include <pficommon/lang/cast.h>

#include "../fv_converter/converter_config.hpp"
#include "../fv_converter/datum.hpp"
#include "../fv_converter/revert.hpp"
#include "../recommender/recommender_factory.hpp"

using namespace pfi::lang;
using std::string;

namespace jubatus {
namespace server {

recommender_serv::recommender_serv(const server_argv& a):
  jubatus_serv<recommender_base,std::string>(a, a.tmpdir)
{
  //  model_ = make_model();

  function<std::string(const recommender_base*)>
    getdiff(&recommender_serv::get_diff);
  function<int(const recommender_base*, const std::string&, std::string&)>
    reduce(&recommender_serv::reduce);
  function<int(recommender_base*, const std::string&)>
    putdiff(&recommender_serv::put_diff);

  set_mixer(getdiff, reduce, putdiff);
}

recommender_serv::~recommender_serv(){
}

int recommender_serv::set_config(config_data config)
{
  config_ = config;
  shared_ptr<fv_converter::datum_to_fv_converter> converter(new fv_converter::datum_to_fv_converter);
  fv_converter::converter_config c;
  convert<jubatus::converter_config, fv_converter::converter_config>(config.converter, c);
  fv_converter::initialize_converter(c, *converter);
  converter_ = converter;

  model_ = make_model();
  return 0;
}
  
config_data recommender_serv::get_config(int)
{
  return config_;
}

int recommender_serv::clear_row(std::string id, int)
{
  ++clear_row_cnt_;
  model_->clear_row(id);
  return 0;
}

int recommender_serv::update_row(std::string id,datum dat)
{
  ++update_row_cnt_;
  fv_converter::datum d;
  convert<jubatus::datum, fv_converter::datum>(dat, d);
  sfv_diff_t v;
  converter_->convert(d, v);
  model_->update_row(id, v);
  return 0;
}

int recommender_serv::clear(int)
{
  clear_row_cnt_ = 0;
  update_row_cnt_ = 0;
  build_cnt_ = 0;
  mix_cnt_ = 0;
  model_->clear();
  return 0;
}

pfi::lang::shared_ptr<recommender_base> recommender_serv::make_model(){
  return pfi::lang::shared_ptr<recommender_base>
    (jubatus::recommender::create_recommender(config_.method));
}  

void recommender_serv::after_load(){
}



datum recommender_serv::complete_row_from_id(std::string id, int)
{
  sfv_t v;
  fv_converter::datum ret;
  model_->complete_row(id, v);

  fv_converter::revert_feature(v, ret);

  datum ret0;
  convert<fv_converter::datum, datum>(ret, ret0);
  return ret0;
}

datum recommender_serv::complete_row_from_data(datum dat)
{
  fv_converter::datum d;
  convert<jubatus::datum, fv_converter::datum>(dat, d);
  sfv_t u, v;
  fv_converter::datum ret;
  converter_->convert(d, u);
  model_->complete_row(u, v);

  fv_converter::revert_feature(v, ret);

  datum ret0;
  convert<fv_converter::datum, datum>(ret, ret0);
  return ret0;
}

similar_result recommender_serv::similar_row_from_id(std::string id, size_t ret_num)
{
  similar_result ret;
  model_->similar_row(id, ret, ret_num);
  return ret;
}

similar_result recommender_serv::similar_row_from_data(std::pair<datum, size_t> data)
{
  similar_result ret;
  fv_converter::datum d;
  convert<datum, fv_converter::datum>(data.first, d);

  sfv_t v;
  converter_->convert(d, v);
  model_->similar_row(v, ret, data.second);
  return ret;
}

datum recommender_serv::decode_row(std::string id, int)
{
  sfv_t v;
  fv_converter::datum ret;

  model_->decode_row(id, v);
  fv_converter::revert_feature(v, ret);
  
  datum ret0;
  convert<fv_converter::datum, datum>(ret, ret0);
  return ret0;
}

std::vector<std::string> recommender_serv::get_all_rows(int)
{
  std::vector<std::string> ret;
  model_->get_all_row_ids(ret);
  return ret;
}

std::string recommender_serv::get_diff(const recommender_base* model){
  std::string ret;
  model->get_const_storage()->get_diff(ret);
  return ret;
}

int recommender_serv::put_diff(recommender_base* model, std::string v){
  model->get_storage()->set_mixed_and_clear_diff(v);
  return 0;
}

int recommender_serv::reduce(const recommender_base* model, const std::string& v, std::string& acc){
  model->get_const_storage()->mix(v, acc);
  return 0;
}

// std::map<std::pair<std::string, int>, std::map<std::string, std::string> > > recommender_serv::get_status(std::string name)
// {
//   std::map<std::string, std::string> ret0;
//   util::get_machine_status(ret0);

//   ret0["clear_row_cnt"] = pfi::lang::lexical_cast<std::string>(clear_row_cnt_);
//   ret0["update_row_cnt"] = pfi::lang::lexical_cast<std::string>(update_row_cnt_);
//   ret0["build_cnt"] = pfi::lang::lexical_cast<std::string>(build_cnt_);
//   ret0["mix_cnt"] = pfi::lang::lexical_cast<std::string>(mix_cnt_);

//   if(ret0.empty()){
//     return std::map<std::pair<string,int>,std::map<std::string,std::string> > >::fail("no result");
//   }else{
//     std::map<std::pair<string,int>, std::map<std::string,std::string> > ret;
//     std::pair<string,int> __hoge__ = make_pair(a_.eth,a_.port); //FIXME
//     ret.insert(make_pair(__hoge__, ret0));
//     return result<std::map<std::pair<string,int>,std::map<std::string,std::string> > >::ok(ret);
//   }
// }


} // namespace recommender
} // namespace jubatus