// --------------------------------------------------------------------------
//           OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2018.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//  notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//  notice, this list of conditions and the following disclaimer in the
//  documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//  may be used to endorse or promote products derived from this software
//  without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Timo Sachsenberg $
// $Authors: Matthias Fuchs $
// --------------------------------------------------------------------------



#include <OpenMS/CONCEPT/Exception.h>
#include <OpenMS/CONCEPT/UniqueIdInterface.h>

#include <OpenMS/DATASTRUCTURES/DBoundingBox.h>
#include <OpenMS/DATASTRUCTURES/DateTime.h>
#include <OpenMS/DATASTRUCTURES/DataValue.h>
#include <OpenMS/DATASTRUCTURES/ListUtils.h>
#include <OpenMS/DATASTRUCTURES/Param.h>
#include <OpenMS/DATASTRUCTURES/String.h>

#include <OpenMS/FORMAT/FeatureSQLFile.h>
#include <OpenMS/FORMAT/FileHandler.h>
#include <OpenMS/FORMAT/SqliteConnector.h>

#include <OpenMS/KERNEL/FeatureMap.h>
#include <OpenMS/KERNEL/ConsensusMap.h>
#include <OpenMS/KERNEL/StandardTypes.h>

#include <OpenMS/METADATA/DataProcessing.h>
#include <OpenMS/METADATA/MetaInfoInterface.h>
#include <OpenMS/METADATA/MetaInfoInterfaceUtils.h>

#include <OpenMS/SYSTEM/File.h>

#include <unordered_set>
#include <sqlite3.h>
#include <sstream>
#include <map>
#include <boost/algorithm/string/join.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>
#include <boost/algorithm/string.hpp>

#include <typeinfo>

// #include <Boost>

namespace OpenMS
{ 
  namespace Sql = Internal::SqliteHelper;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //                                                            helper functions                                                          //
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  // store functions
  // convert int of enum datatype to a struct with prefix and type 
  PrefixSQLTypePair enumToPrefix(const DataValue::DataType& dt)
  {
    // create label prefix according to type
    String prefix;
    String type;
    PrefixSQLTypePair pSTP;
    switch (dt)
    {
      case DataValue::STRING_VALUE:
        pSTP.prefix = "_S_";
        pSTP.sqltype = "TEXT";
        break;
      case DataValue::INT_VALUE:
        pSTP.prefix = "_I_";
        pSTP.sqltype = "INTEGER";
        break;
      case DataValue::DOUBLE_VALUE:
        pSTP.prefix = "_D_";
        pSTP.sqltype = "FLOAT";
        break;
      case DataValue::STRING_LIST:
        pSTP.prefix = "_SL_";
        pSTP.sqltype = "TEXT";
        break;
      case DataValue::INT_LIST:
        pSTP.prefix = "_IL_";
        pSTP.sqltype = "TEXT";
        break;
      case DataValue::DOUBLE_LIST:
        pSTP.prefix = "_DL_";
        pSTP.sqltype = "TEXT";
        break;
      case DataValue::EMPTY_VALUE:
        pSTP.prefix = "";
        pSTP.sqltype = "TEXT";
        break;
    }
    return pSTP;
  }

  String createTable(const String& table_name, const String& table_stmt)
  {
    String create_table_stmt = "CREATE TABLE " + table_name + " (" + table_stmt + ");";
    return create_table_stmt;
  }

  // get datatype
  DataValue::DataType getColumnDatatype(const  String& label)
  { 
    DataValue::DataType type;
    if (label.hasPrefix("_S_"))
    {
      type = DataValue::STRING_VALUE;
    } else if (label.hasPrefix("_I_"))
    {
      type = DataValue::INT_VALUE;
    } else if (label.hasPrefix("_D_"))
    {
      type = DataValue::DOUBLE_VALUE;      
    } else if (label.hasPrefix("_SL_"))
    {
      type = DataValue::STRING_LIST;
    } else if (label.hasPrefix("_IL_"))
    {
      type = DataValue::INT_LIST;
    } else if (label.hasPrefix("_DL_"))
    {
      type = DataValue::DOUBLE_LIST;
    } else 
    {
      type = DataValue::EMPTY_VALUE;
      /*
      if (!label.hasPrefix("_"))
      {
        type = 6;
      }
      */
    }
    return type;
  }

  /*
    // get userparameter
    String getColumnName(const String& label)
    { 
      String label_userparam;
      if (label.hasPrefix("_S_"))
      {
        label_userparam = label.substr(3);
      } else if (label.hasPrefix("_I_"))
      {
        label_userparam = label.substr(3);
      } else if (label.hasPrefix("_D_"))
      {
        label_userparam = label.substr(3);
      } else if (label.hasPrefix("_SL_"))
      {
        label_userparam = label.substr(4);
      } else if (label.hasPrefix("_IL_"))
      {
        label_userparam = label.substr(4);
      } else if (label.hasPrefix("_DL_"))
      {
        label_userparam = label.substr(4);
      } else 
      {
        label_userparam = label;
        
        //if (!label.hasPrefix("_"))
        //{
        //  datatype = 6;
        //}
        
      }
      return label_userparam;
    }
  */

  // write userparameter to feature
  void setUserParams(Feature& current_feature, String &column_name, const DataValue::DataType column_type, int i, sqlite3_stmt* stmt)
  {
    if (column_type == DataValue::STRING_VALUE)
    {
      column_name = column_name.substr(3);
      String value;
      Sql::extractValue<String>(&value, stmt, i);
      current_feature.setMetaValue(column_name, value); 
    } 
      else if (column_type == DataValue::INT_VALUE)
    {
      column_name = column_name.substr(3);
      int value = 0;
      Sql::extractValue<int>(&value, stmt, i);
      current_feature.setMetaValue(column_name, value); 
    } else if (column_type == DataValue::DOUBLE_VALUE)
    {
      column_name = column_name.substr(3);
      double value = 0.0;
      Sql::extractValue<double>(&value, stmt, i);          
      current_feature.setMetaValue(column_name, value); 
    } else if (column_type == DataValue::STRING_LIST)
    {
      column_name = column_name.substr(4);
      String value; 
      Sql::extractValue<String>(&value, stmt, i);

      StringList sl;
      // cut off "[" and "]""
      value = value.chop(1);
      value = value.substr(1);
      value.split(", ", sl);
      current_feature.setMetaValue(column_name, sl);
    } else if (column_type == DataValue::INT_LIST)
    {
      column_name = column_name.substr(4);
      String value; //IntList value;
      Sql::extractValue<String>(&value, stmt, i); //IntList
      value = value.chop(1);
      value = value.substr(1);
      std::vector<String> value_list;
      IntList il = ListUtils::create<int>(value, ',');
      current_feature.setMetaValue(column_name, il);
    } else if (column_type == DataValue::DOUBLE_LIST)
    {
      column_name = column_name.substr(4);
      String value; //DoubleList value;
      Sql::extractValue<String>(&value, stmt, i); //DoubleList
      value = value.chop(1);
      value = value.substr(1);          
      DoubleList dl = ListUtils::create<double>(value, ',');
      current_feature.setMetaValue(column_name, dl);
    } else if (column_type == DataValue::EMPTY_VALUE)
    {
      String value;
      Sql::extractValue<String>(&value, stmt, i);
    }
  }

  // probe first feature in feature_map as template to set tables
  // check once for features, subordinates, dataprocessing, convexhull bboxes respectively 
  std::tuple<bool, bool, bool, bool, bool> getTables(const FeatureMap& feature_map)
  {
    bool features_switch, subordinates_switch, dataprocessing_switch, features_bbox_switch, subordinates_bbox_switch;
    features_switch = false;
    features_bbox_switch = false;
    subordinates_switch = false;
    subordinates_bbox_switch = false;
    dataprocessing_switch = false;
    if (feature_map.getDataProcessing().size() > 0)
    {
      dataprocessing_switch = true;
    }
    for (auto feature : feature_map)
    {
      features_switch = true;
      if (feature.getConvexHulls().size() > 0)
      {
        features_bbox_switch = true;
      }
      for (Feature sub : feature.getSubordinates())
      {
        subordinates_switch = true;
        if (sub.getConvexHulls().size() > 0)
        {
          subordinates_bbox_switch = true;
        }
        break;
      }
      break;
    }
    return std::make_tuple(features_switch, subordinates_switch, dataprocessing_switch, features_bbox_switch, subordinates_bbox_switch);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //                                                store FeatureMap as SQL database                                                      //
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // fitted snippet of FeatureXMLFile::load
  void FeatureSQLFile::write(const std::string& out_fm, const FeatureMap& feature_map) const
  {
    String path="/home/mkf/Development/OpenMS/src/tests/class_tests/openms/data/";
    String filename = path.append(out_fm);
    
    // delete file if present
    File::remove(filename);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                                        variable declaration                                                          //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////   

    // declare table entries
    std::vector<String> feature_elements = {"ID", "RT", "MZ", "Intensity", "Charge", "Quality"};
    std::vector<String> feature_elements_types = {"INTEGER", "REAL", "REAL", "REAL", "INTEGER", "REAL"};
    
    std::vector<String> subordinate_elements = {"ID", "SUB_IDX" , "REF_ID", "RT", "MZ", "Intensity", "Charge", "Quality"};
    std::vector<String> subordinate_elements_types = {"INTEGER", "INTEGER", "INTEGER", "REAL", "REAL", "REAL", "INTEGER", "REAL"};

    std::vector<String> dataprocessing_elements = {"ID", "SOFTWARE", "SOFTWARE_VERSION", "DATA", "TIME", "ACTIONS"};
    std::vector<String> dataprocessing_elements_types = {"INTEGER" ,"TEXT" ,"TEXT" ,"TEXT" ,"TEXT" , "TEXT"};

    std::vector<String> feat_bounding_box_elements = {"REF_ID", "MIN_MZ", "MIN_RT", "MAX_MZ", "MAX_RT", "BB_IDX"};
    std::vector<String> feat_bounding_box_elements_types = {"INTEGER" ,"REAL" ,"REAL" ,"REAL" , "REAL", "INTEGER"};

    std::vector<String> sub_bounding_box_elements = {"ID", "REF_ID", "MIN_MZ", "MIN_RT", "MAX_MZ", "MAX_RT", "BB_IDX"};
    std::vector<String> sub_bounding_box_elements_types = {"INTEGER" ,"INTEGER" ,"REAL" ,"REAL" ,"REAL" , "REAL", "INTEGER"};

    // get used tables
    auto tables = getTables(feature_map);
    bool features_switch = std::get<0>(tables);
    bool subordinates_switch = std::get<1>(tables);
    bool dataprocessing_switch = std::get<2>(tables);
    bool features_bbox_switch = std::get<3>(tables);
    bool subordinates_bbox_switch = std::get<4>(tables);

    std::cout << features_switch << "\t" << subordinates_switch << "\t" <<  dataprocessing_switch << "\t" << features_bbox_switch << "\t" << subordinates_bbox_switch << std::endl; 

    // read feature_map userparameter values and store as key value map
    // pass all CommonMetaKeys by setting frequency to 0.0 to a set 
    std::set<String> common_keys = MetaInfoInterfaceUtils::findCommonMetaKeys<FeatureMap, std::set<String> >(feature_map.begin(), feature_map.end(), 0.0);
    std::map<String, DataValue::DataType> map_key2type;

    for (auto feature : feature_map)
    {
      for (const String& key : common_keys)
      {
        if (feature.metaValueExists(key))
        {
          const DataValue::DataType& dt = feature.getMetaValue(key).valueType();
          map_key2type[key] = dt;
        }
      }
    }

    std::vector<String> dataproc_keys;
    const std::vector<DataProcessing> dataprocessing_userparams = feature_map.getDataProcessing();
    std::map<String, DataValue::DataType> dataproc_map_key2type;

    for (auto datap : dataprocessing_userparams)
    {
      datap.getKeys(dataproc_keys);
      for (auto key : dataproc_keys)
      {
        const DataValue::DataType& dt = datap.getMetaValue(key).valueType(); 
        dataproc_map_key2type[key] = dt;

        /*
        const DataValue::DataType& dt = datap.getMetaValue(key).valueType(); 
        if ((dt == DataValue::STRING_VALUE) || (dt == DataValue::STRING_LIST))
        {
          dataproc_map_key2type[key] = dataproc_map_key2type["\"" + key + "\""];
        }
        dataproc_map_key2type.erase(key);
        */

      }
    }


    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// build feature header for sql table                                                                                                  //
    /// build subordinate header as sql table                                                                                               //
    /// build dataprocessing header as sql table                                                                                            //
    /// build boundingbox header as sql table                                                                                               //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // prepare feature header, add (dynamic) part of user_parameter labels to header and header type vector 
    for (const String& key : common_keys)
    {
      // feature_elements vector with strings prefix (_TYPE_, _S_, _IL_, ...) and key  
      feature_elements.push_back(enumToPrefix(map_key2type[key]).prefix + key);
      // feature_elements vector with SQL TYPES 
      feature_elements_types.push_back(enumToPrefix(map_key2type[key]).sqltype);
    }

    // prepare subordinate header
    std::map<String, DataValue::DataType> subordinate_key2type;
    for (FeatureMap::const_iterator feature_it = feature_map.begin(); feature_it != feature_map.end(); ++feature_it)
    {
      for (std::vector<Feature>::const_iterator sub_it = feature_it->getSubordinates().begin(); sub_it != feature_it->getSubordinates().end(); ++sub_it)
      { 
        std::vector<String> keys;
        sub_it->getKeys(keys);
        for (const String& key : keys)
        {
          subordinate_key2type[key] = sub_it->getMetaValue(key).valueType();
        }
      }
    }
    for (const auto& key2type : subordinate_key2type)
    {
      // subordinate_elements vector with strings prefix (_TYPE_, _S_, _IL_, ...) and key  
      subordinate_elements.push_back(enumToPrefix(subordinate_key2type[key2type.first]).prefix + key2type.first);
      // subordinate_elements_type vector with SQL TYPES 
      subordinate_elements_types.push_back(enumToPrefix(subordinate_key2type[key2type.second]).sqltype);
    }

    // prepare dataprocessing header
    for (const auto& key2type : dataproc_map_key2type)
    {
      // dataprocessing vector with strings prefix (_TYPE_, _S_, _IL_, ...) and key  
      dataprocessing_elements.push_back(enumToPrefix(dataproc_map_key2type[key2type.first]).prefix + key2type.first);
      // dataprocessing_elements_type vector with SQL TYPES 
      dataprocessing_elements_types.push_back(enumToPrefix(key2type.second).sqltype);
    }
    // test for illegal entries
    // catch :
    const std::vector<String> bad_sym = {"+", "_", "-", "?", "!", "*", "@", "%", "^", "&", "#", "=", "/", "\\", ":", "\"", "\'"};

    for (std::size_t idx = 0; idx != dataprocessing_elements.size(); ++idx)
    {
      // test for illegal SQL entries in dataprocessing_elements, if found replace
      for (const String& sym : bad_sym)
      {
        if (dataprocessing_elements[idx].hasSubstring(sym))
        // use String::quote
        {
          dataprocessing_elements[idx] = dataprocessing_elements[idx].quote();
          break;
        }
      }
    }

    // boundingbox is part of convexhull is part of feature and subordinates
    // points of MZ/RT-min-/maxima saved as quadrupel of doubles per convexhull number
    // preparation of boundingbox header atm needs no implementation





    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                                     create database with empty tables                                                //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // construct SQL_labels for feature, subordinate, dataprocessing and boundingbox tables
    // create empty
    // 1. features table
    // vector sql_labels, concatenate element and respective SQL type
    std::vector<String> sql_labels_features = {};
    for (std::size_t idx = 0; idx != feature_elements.size(); ++idx)
    {
      sql_labels_features.push_back(feature_elements[idx] + " " + feature_elements_types[idx]);
    }
    // add PRIMARY KEY to first entry, assuming 1st column contains primary key
    sql_labels_features[0].append(" PRIMARY KEY");
    // add "NOT NULL" to all entries
    std::for_each(sql_labels_features.begin(), sql_labels_features.end(), [] (String &s) {s.append(" NOT NULL");});
    // create single string with delimiter ',' as TABLE header template
    String sql_stmt_features = ListUtils::concatenate(sql_labels_features, ",");


    // 2. subordinates table
    // vector sql_labels, concatenate element and respective SQL type
    std::vector<String> sql_labels_subordinates = {};
    for (std::size_t idx = 0; idx != subordinate_elements.size(); ++idx)
    {
      sql_labels_subordinates.push_back(subordinate_elements[idx] + " " + subordinate_elements_types[idx]);
    }
    // add PRIMARY KEY to first entry, assuming 1st column contains primary key
    sql_labels_subordinates[0].append(" PRIMARY KEY");
    // add "NOT NULL" to all entries
    std::for_each(sql_labels_subordinates.begin(), sql_labels_subordinates.end(), [] (String &s) {s.append(" NOT NULL");});
    // create single string with delimiter ',' as TABLE header template
    String sql_stmt_subordinates = ListUtils::concatenate(sql_labels_subordinates, ",");


    // 3. dataprocessing table
    std::vector<String> sql_labels_dataprocessing = {};
    for (std::size_t idx = 0; idx != dataprocessing_elements.size(); ++idx)
    {
      sql_labels_dataprocessing.push_back(dataprocessing_elements[idx] + " " + dataprocessing_elements_types[idx]);
    }
    // add PRIMARY KEY to first entry, assuming 1st column contains primary key
    sql_labels_dataprocessing[0].append(" PRIMARY KEY");
    // add "NOT NULL" to all entries
    std::for_each(sql_labels_dataprocessing.begin(), sql_labels_dataprocessing.end(), [] (String &s) {s.append(" NOT NULL");});
    // create single string with delimiter ',' as TABLE header template
    String sql_stmt_dataprocessing = ListUtils::concatenate(sql_labels_dataprocessing, ",");


    // 4. boundingbox table
    // 4.1 features
    std::vector<String> sql_labels_boundingbox = {};
    for (std::size_t idx = 0; idx != feat_bounding_box_elements.size(); ++idx)
    {
      sql_labels_boundingbox.push_back(feat_bounding_box_elements[idx] + " " + feat_bounding_box_elements_types[idx]);
    }
    // no PRIMARY KEY because of executeStatement's UNIQUE constraint for primary keys
    // add "NOT NULL" to all entries
    std::for_each(sql_labels_boundingbox.begin(), sql_labels_boundingbox.end(), [] (String &s) {s.append(" NOT NULL");});
    // create single string with delimiter ',' as TABLE header template
    String sql_stmt_feat_boundingbox = ListUtils::concatenate(sql_labels_boundingbox, ",");


    // 4. boundingbox table
    // 4.2 subordinates
    sql_labels_boundingbox = {};
    for (std::size_t idx = 0; idx != sub_bounding_box_elements.size(); ++idx)
    {
      sql_labels_boundingbox.push_back(sub_bounding_box_elements[idx] + " " + sub_bounding_box_elements_types[idx]);
    }
    // add PRIMARY KEY to first entry, assuming 1st column contains primary key
    //sql_labels_boundingbox[0].append(" PRIMARY KEY");
    // add "NOT NULL" to all entries
    std::for_each(sql_labels_boundingbox.begin(), sql_labels_boundingbox.end(), [] (String &s) {s.append(" NOT NULL");});
    // create single string with delimiter ',' as TABLE header template
    String sql_stmt_sub_boundingbox = ListUtils::concatenate(sql_labels_boundingbox, ",");


    // create empty SQL table stmt
    String features_table_stmt, subordinates_table_stmt, dataprocessing_table_stmt, feature_boundingbox_table_stmt, subordinate_boundingbox_table_stmt;
    // 1. features
    if (features_switch == true)
    {
      features_table_stmt = createTable("FEATURES_TABLE", sql_stmt_features);
    } else if (features_switch == false)
    {
      features_table_stmt = "";
    }
    // 2. subordinates
    if (subordinates_switch == true)
    {
      subordinates_table_stmt = createTable("FEATURES_SUBORDINATES", sql_stmt_subordinates);
    } else if (subordinates_switch == false)
    {
      subordinates_table_stmt = "";
    }
    // 3. dataprocessing
    if (dataprocessing_switch == true)
    {
      dataprocessing_table_stmt = createTable("FEATURES_DATAPROCESSING", sql_stmt_dataprocessing);
    } else if (dataprocessing_switch == false)
    {
      dataprocessing_table_stmt = "";
    }
    // 4. boundingbox (features & subordinates)
    // features
    if (features_bbox_switch == true)
    {
      feature_boundingbox_table_stmt = createTable("FEATURES_TABLE_BOUNDINGBOX", sql_stmt_feat_boundingbox);
    } else if (features_bbox_switch == false)
    {
      feature_boundingbox_table_stmt = "";
    }
    // subordinates
    if (subordinates_bbox_switch)
    {
      subordinate_boundingbox_table_stmt = createTable("SUBORDINATES_TABLE_BOUNDINGBOX", sql_stmt_sub_boundingbox);
    } else if (subordinates_bbox_switch == false)
    {
      subordinate_boundingbox_table_stmt = "";
    }


    String create_sql = \
      features_table_stmt + \
      subordinates_table_stmt + \
      dataprocessing_table_stmt + \
      feature_boundingbox_table_stmt + \
      subordinate_boundingbox_table_stmt \
      ;    

    // catch SQL contingencies
    //create_sql = create_sql.substitute("'", "\:"); // SQL escape single quote in strings  
    // create SQL database, create empty SQL tables  see (https://github.com/OpenMS/OpenMS/blob/develop/src/openms/source/FORMAT/OSWFile.cpp)
    // Open connection to database
    SqliteConnector conn(filename);
    conn.executeStatement(create_sql);





































    // break into functions?
    // turn into line feed function for SQL database input step statement
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                                     store FeatureMap data in tables                                                  //
    //                                                     1. features                                                                      //                                    
    //                                                     2. subordinates                                                                  //
    //                                                     3. dataprocessing                                                                //
    //                                                     4. boundingboxes features & subordinates                                         //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// 1. insert data of features table
    const String feature_elements_sql_stmt = ListUtils::concatenate(feature_elements, ","); 
    const String feat_bbox_elements_sql_stmt = ListUtils::concatenate(feat_bounding_box_elements, ","); 

    if (features_switch)
    {
      for (const Feature& feature : feature_map)
      {
        String line_stmt_features_table;
        std::vector<String> line;

        int64_t id = static_cast<int64_t>(feature.getUniqueId() & ~(1ULL << 63));
        line.push_back(id);
        line.push_back(feature.getRT());
        line.push_back(feature.getMZ());
        line.push_back(feature.getIntensity());
        line.push_back(feature.getCharge());
        line.push_back(feature.getOverallQuality());
        for (const String& key : common_keys)
        {
          String s = feature.getMetaValue(key);
          if (map_key2type[key] == DataValue::STRING_VALUE
          || map_key2type[key] == DataValue::STRING_LIST)
          {
            s = s.substitute("'", "''"); // SQL escape single quote in strings  
          }

          if (map_key2type[key] == DataValue::STRING_VALUE
          || map_key2type[key] == DataValue::STRING_LIST
          || map_key2type[key] == DataValue::INT_LIST
          || map_key2type[key] == DataValue::DOUBLE_LIST)
          {
            s = "'" + s + "'"; // quote around SQL strings (and list types)
          }
          line.push_back(s);
        }
        line_stmt_features_table =  "INSERT INTO FEATURES_TABLE (" + feature_elements_sql_stmt + ") VALUES (";
        line_stmt_features_table += ListUtils::concatenate(line, ",");
        line_stmt_features_table += ");";
        //store in features table
        conn.executeStatement(line_stmt_features_table);
      }
    }

    if (features_bbox_switch)
    {
      // fetch convexhull of feature for bounding box data
      for (const Feature& feature : feature_map)
      {
        /// insert boundingbox data to table FEATURES_TABLE_BOUNDINGBOX
        std::vector<String> feat_bbox_line;

        double min_MZ;
        double min_RT;
        double max_MZ;
        double max_RT;

        int bbox_idx = 0;
        int64_t id = static_cast<int64_t>(feature.getUniqueId() & ~(1ULL << 63));

        // add bbox entries of current convexhull
        for (Size b = 0; b < feature.getConvexHulls().size(); ++b)
        {
          //it->getConvexHulls()[i].expandToBoundingBox();

          //clear vector line for current bbox
          feat_bbox_line.clear();
          String line_stmt_features_table_boundingbox =  "INSERT INTO FEATURES_TABLE_BOUNDINGBOX (" + feat_bbox_elements_sql_stmt + ") VALUES (";
          bbox_idx = b;

          min_MZ = feature.getConvexHulls()[b].getBoundingBox().minX();
          min_RT = feature.getConvexHulls()[b].getBoundingBox().minY();
          max_MZ = feature.getConvexHulls()[b].getBoundingBox().maxX();
          max_RT = feature.getConvexHulls()[b].getBoundingBox().maxY();

          feat_bbox_line.push_back(id);
          feat_bbox_line.insert(feat_bbox_line.end(), {min_MZ,min_RT,max_MZ,max_RT});
          feat_bbox_line.push_back(bbox_idx);

          line_stmt_features_table_boundingbox += ListUtils::concatenate(feat_bbox_line, ",");
          //store String in FEATURES_TABLE_BOUNDINGBOX
          line_stmt_features_table_boundingbox += ");";
          conn.executeStatement(line_stmt_features_table_boundingbox);
        }
      }
    }

    if (subordinates_switch)
    {
      /// 2. insert data of subordinates table
      String line_stmt;
      const String subordinate_elements_sql_stmt = ListUtils::concatenate(subordinate_elements, ","); 
      //for (auto feature : feature_map)
      for (const Feature& feature : feature_map)
      {
        int sub_idx = 0;
        std::vector<String> line;
        for (const Feature& sub : feature.getSubordinates())
        {          
          line.push_back(static_cast<int64_t>(sub.getUniqueId() & ~(1ULL << 63)));
          // additional index value to preserve order of subordinates
          line.push_back(sub_idx);
          ++sub_idx;
          line.push_back(static_cast<int64_t>(feature.getUniqueId() & ~(1ULL << 63)));
          line.push_back(sub.getRT());
          line.push_back(sub.getMZ());
          line.push_back(sub.getIntensity());
          line.push_back(sub.getCharge());
          line.push_back(sub.getOverallQuality());
          for (const std::pair<String, DataValue::DataType> k2t : subordinate_key2type)
          {
            const String& key = k2t.first;
            if (sub.metaValueExists(key))
            {
              String s = sub.getMetaValue(k2t.first);
              if (k2t.second == DataValue::STRING_VALUE
              || k2t.second == DataValue::STRING_LIST)
              {
                s = s.substitute("'", "''"); // SQL escape single quote in strings  
              }

              if (k2t.second == DataValue::STRING_VALUE
              || k2t.second == DataValue::STRING_LIST
              || k2t.second == DataValue::INT_LIST
              || k2t.second == DataValue::DOUBLE_LIST)
              {
                s = "'" + s + "'"; // quote around SQL strings (and list types)
              }
              line.push_back(s);
            }
          }
          line_stmt =  "INSERT INTO FEATURES_SUBORDINATES (" + subordinate_elements_sql_stmt + ") VALUES (";
          line_stmt += ListUtils::concatenate(line, ",");
          line_stmt += ");";
          //store in subordinates table
          conn.executeStatement(line_stmt);
          //clear vector line
          line.clear();
        }
      }
    }

    if (subordinates_bbox_switch)
    {
      // fetch bounding box data of subordinate convexhull
      const String sub_bbox_elements_sql_stmt = ListUtils::concatenate(sub_bounding_box_elements, ","); 
      for (const Feature& feature : feature_map)
      {
        int64_t ref_id = static_cast<int64_t>(feature.getUniqueId() & ~(1ULL << 63));

        for (const Feature& sub : feature.getSubordinates())
        {
          int64_t id = static_cast<int64_t>(sub.getUniqueId() & ~(1ULL << 63));

          /// insert boundingbox data to table SUBORDINATES_TABLE_BOUNDINGBOX
          std::vector<String> bbox_line;

          double min_MZ;
          double min_RT;
          double max_MZ;
          double max_RT;

          int bbox_idx = 0;

          // add bbox entries of current convexhull
          for (Size b = 0; b < sub.getConvexHulls().size(); ++b)
          {
            //clear vector line for current bbox
            bbox_line.clear();
            String line_stmt_subordinates_table_boundingbox =  "INSERT INTO SUBORDINATES_TABLE_BOUNDINGBOX (" + sub_bbox_elements_sql_stmt + ") VALUES (";
            bbox_idx = b;

            min_MZ = sub.getConvexHulls()[b].getBoundingBox().minX();
            min_RT = sub.getConvexHulls()[b].getBoundingBox().minY();
            max_MZ = sub.getConvexHulls()[b].getBoundingBox().maxX();
            max_RT = sub.getConvexHulls()[b].getBoundingBox().maxY();

            bbox_line.push_back(id);
            bbox_line.push_back(ref_id);
            bbox_line.insert(bbox_line.end(), {min_MZ,min_RT,max_MZ,max_RT});
            bbox_line.push_back(bbox_idx);

            line_stmt_subordinates_table_boundingbox += ListUtils::concatenate(bbox_line, ",");
            line_stmt_subordinates_table_boundingbox += ");";
            conn.executeStatement(line_stmt_subordinates_table_boundingbox);
          }
        }
      }
    }

    if (dataprocessing_switch)
    {
      /// 3. insert data of dataprocessing table
      const String dataprocessing_elements_sql_stmt = ListUtils::concatenate(dataprocessing_elements, ","); 
      std::vector<String> dataproc_elems = {};
      std::vector<String> processing_action_names;
      
      std::cout << "\n" << static_cast<int64_t>(feature_map.getUniqueId() & ~(1ULL << 63)) << std::endl;
      dataproc_elems.push_back(static_cast<int64_t>(feature_map.getUniqueId() & ~(1ULL << 63)));
      
      const std::vector<DataProcessing> dataprocessing = feature_map.getDataProcessing();
    
      for (auto datap : dataprocessing)
      {
        dataproc_elems.push_back(datap.getSoftware().getName());
        dataproc_elems.push_back(datap.getSoftware().getVersion());
        dataproc_elems.push_back(datap.getCompletionTime().getDate());
        dataproc_elems.push_back(datap.getCompletionTime().getTime());

        const std::set<DataProcessing::ProcessingAction>& proc_ac = datap.getProcessingActions();

        for (const DataProcessing::ProcessingAction& a : proc_ac)
        {
          const String& action = DataProcessing::NamesOfProcessingAction[int(a)];
          processing_action_names.push_back(action);
        }
        if (processing_action_names.size() >= 1)
        {
          dataproc_elems.push_back(ListUtils::concatenate(processing_action_names, ",")); 
        } else
        {
          dataproc_elems.push_back(processing_action_names[0]);
        }
      }
    
      // userparam entries
      // dynamic type resolution 
      for (auto datap : dataprocessing_userparams)
      {
        datap.getKeys(dataproc_keys);
        for (auto key : dataproc_keys)
        {
          dataproc_elems.push_back(datap.getMetaValue(key));
        }
      }

      // non-userparam entries
      // resolve SQL type from dataprocessing_elements_types
      for (std::size_t idx = 0; idx != dataprocessing_elements.size(); ++idx)
      {
        if (dataprocessing_elements_types[idx] == "TEXT")
        {
          dataproc_elems[idx] = "'" + dataproc_elems[idx] + "'"; 
        }
      }
      String line_stmt =  "INSERT INTO FEATURES_DATAPROCESSING (" + dataprocessing_elements_sql_stmt + ") VALUES (";
      line_stmt += ListUtils::concatenate(dataproc_elems, ",");
      line_stmt += ");";
      //store in dataprocessing table
      conn.executeStatement(line_stmt);
    }

  } // end of FeatureSQLFile::write













































































































  // alternate version with table join 
  /*
    // access of features and boundingboxes
    SELECT * FROM  FEATURES_TABLE LEFT JOIN FEATURES_TABLE_BOUNDINGBOX ON FEATURES_TABLE.id = FEATURES_TABLE_BOUNDINGBOX.ref_id
    // access of features, subordinates and boundingboxes
    SELECT * FROM  FEATURES_TABLE LEFT JOIN FEATURES_SUBORDINATES ON FEATURES_TABLE.id = FEATURES_SUBORDINATES.ref_id LEFT JOIN SUBORDINATES_TABLE_BOUNDINGBOX ON FEATURES_SUBORDINATES.id = SUBORDINATES_TABLE_BOUNDINGBOX.id

  // working modell
  read number of columns for all tables except dataprocessing
  combine access to joined tables via compound index
  one big loop with two different statements
  1. for features access
  2. for subordinate access
  */

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //                                                   read FeatureMap as SQL database                                                    //
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  // read SQL database and store as FeatureMap
  // fitted snippet of FeatureXMLFile::load
  // based on TransitionPQPFile::readPQPInput

  FeatureMap FeatureSQLFile::read(const std::string& filename) const
  {
    //create empty FeatureMap object
    FeatureMap feature_map;
    sqlite3 *db;
    sqlite3_stmt * stmt;
    std::string select_sql;

    // Open database
    SqliteConnector conn(filename);
    db = conn.getDB();

    // set switches to access only existent tables
    bool features_switch = SqliteConnector::tableExists(db, "FEATURES_TABLE");
    bool subordinates_switch = SqliteConnector::tableExists(db, "FEATURES_SUBORDINATES");
    bool dataprocessing_switch = SqliteConnector::tableExists(db, "FEATURES_DATAPROCESSING");
    bool features_bbox_switch = SqliteConnector::tableExists(db, "FEATURES_TABLE_BOUNDINGBOX");
    bool subordinates_bbox_switch = SqliteConnector::tableExists(db, "SUBORDINATES_TABLE_BOUNDINGBOX");

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                                     store sqlite3 database as FeatureMap                                             //
    //                                                     1. features                                                                      //                                    
    //                                                     2. subordinates                                                                  //
    //                                                     3. boundingbox                                                                   //
    //                                                     4. dataprocessing                                                                //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    // check indexing

    String sql;

    // get number of columns for each table except dataprocessing
    // 1. features
    sql = "SELECT * FROM FEATURES_TABLE;";
    SqliteConnector::prepareStatement(db, &stmt, sql);
    //SqliteConnector::executePreparedStatement(db, &stmt, sql);
    sqlite3_step(stmt);
    int cols_features = sqlite3_column_count(stmt);
    std::cout << "\n" << "Number of columns = " << cols_features << std::endl;
    sqlite3_finalize(stmt);

    // 2. subordinates
    sql = "SELECT * FROM FEATURES_SUBORDINATES;";
    SqliteConnector::prepareStatement(db, &stmt, sql);
    //SqliteConnector::executePreparedStatement(db, &stmt, sql);
    sqlite3_step(stmt);
    int cols_subordinates = sqlite3_column_count(stmt);
    std::cout << "Number of columns = " << cols_subordinates << std::endl;
    sqlite3_finalize(stmt);

    // 3. features bbox
    sql = "SELECT * FROM FEATURES_TABLE_BOUNDINGBOX;";
    SqliteConnector::prepareStatement(db, &stmt, sql);
    //SqliteConnector::executePreparedStatement(db, &stmt, sql);
    sqlite3_step(stmt);
    int cols_features_bbox = sqlite3_column_count(stmt);
    std::cout << "Number of columns = " << cols_features_bbox << std::endl;
    sqlite3_finalize(stmt);

    // 4. subordinate bbox
    sql = "SELECT * FROM SUBORDINATES_TABLE_BOUNDINGBOX;";
    SqliteConnector::prepareStatement(db, &stmt, sql);
    //SqliteConnector::executePreparedStatement(db, &stmt, sql);
    sqlite3_step(stmt);
    int cols_subordinates_bbox = sqlite3_column_count(stmt);
    std::cout << "Number of columns = " << cols_subordinates_bbox << std::endl;
    sqlite3_finalize(stmt);

    // 5. concatenation features bbox
    sql = "SELECT * FROM  FEATURES_TABLE LEFT JOIN FEATURES_TABLE_BOUNDINGBOX ON FEATURES_TABLE.id = FEATURES_TABLE_BOUNDINGBOX.ref_id;";
    SqliteConnector::prepareStatement(db, &stmt, sql);
    //SqliteConnector::executePreparedStatement(db, &stmt, sql);
    sqlite3_step(stmt);
    int cols_features_join_bbox = sqlite3_column_count(stmt);
    std::cout << "Number of columns = (features+bbox) " << cols_features_join_bbox << std::endl;
    sqlite3_finalize(stmt);


    // 6. concatenation features subordinates bbox 
    sql = "SELECT * FROM  FEATURES_TABLE LEFT JOIN FEATURES_SUBORDINATES ON FEATURES_TABLE.id = FEATURES_SUBORDINATES.ref_id LEFT JOIN SUBORDINATES_TABLE_BOUNDINGBOX ON FEATURES_SUBORDINATES.id = SUBORDINATES_TABLE_BOUNDINGBOX.id ORDER BY FEATURES_TABLE.ID;";
    SqliteConnector::prepareStatement(db, &stmt, sql);
    //SqliteConnector::executePreparedStatement(db, &stmt, sql);
    sqlite3_step(stmt);
    int cols_features_join_subordinates_join_bbox = sqlite3_column_count(stmt);
    std::cout << "Number of columns = (Features + Subordinates + bbox) " << cols_features_join_subordinates_join_bbox << std::endl;
    sqlite3_finalize(stmt);


    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                                     store sqlite3 database as FeatureMap                                             //
    // traverse queries                                                                                                                     //
    // set all parameters: features, subordinates, boundingboxes                                                                            //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // TODO: case of empty fields

  /*
    // features
    if (features_switch)
    {
      /// 1. get feature data from database
      String sql = "SELECT * FROM  FEATURES_TABLE LEFT JOIN FEATURES_TABLE_BOUNDINGBOX ON FEATURES_TABLE.id = FEATURES_TABLE_BOUNDINGBOX.ref_id;";
      SqliteConnector::prepareStatement(db, &stmt, sql);
      //SqliteConnector::executePreparedStatement(db, &stmt, sql);
      sqlite3_step(stmt);

      while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
      {
        Feature feature;

        int bbox_col = cols_features + 1 + 4; //56 in features + 1 offset id + 4 position of bbox_idx column
        int bbox_idx = 0;
        Sql::extractValue<int>(&bbox_idx, stmt, bbox_col);

        if (bbox_idx == 0)
        {
          if (feature != Feature()) feature_map.push_back(feature);
          feature = Feature();

          //std::cout << "\n bbox_idx = " << bbox_idx << std::endl;
          
          // set feature parameters
          String id_s;
          long id = 0;
          // extract as String TODO
          Sql::extractValue<String>(&id_s, stmt, 0);
          id = stol(id_s);
          double rt = 0.0;
          Sql::extractValue<double>(&rt, stmt, 1);
          double mz = 0.0;
          Sql::extractValue<double>(&mz, stmt, 2);
          double intensity = 0.0;
          Sql::extractValue<double>(&intensity, stmt, 3);
          int charge = 0;
          Sql::extractValue<int>(&charge, stmt, 4);
          double quality = 0.0;
          Sql::extractValue<double>(&quality, stmt, 5);

          // get values id, RT, MZ, Intensity, Charge, Quality
          // save SQL column elements as feature
          feature.setUniqueId(id);
          feature.setRT(rt);
          feature.setMZ(mz);
          feature.setIntensity(intensity);
          feature.setCharge(charge);
          feature.setOverallQuality(quality);

          // index to account for offsets in joined tables
          int col_nr = 6;
          // save userparam columns
          for (int i = col_nr; i < cols_features ; ++i)
          {
            String column_name = sqlite3_column_name(stmt, i);
            int column_type = getColumnDatatype(column_name);

            //std::cout << "\n" << column_name << std::endl;

            if (column_type == DataValue::STRING_VALUE)
            {
              column_name = column_name.substr(3);
              String value;
              Sql::extractValue<String>(&value, stmt, i);
              feature.setMetaValue(column_name, value); 
              continue;
            } else if (column_type == DataValue::INT_VALUE)
            {
              column_name = column_name.substr(3);
              int value = 0;
              Sql::extractValue<int>(&value, stmt, i);
              feature.setMetaValue(column_name, value); 
              continue;
            } else if (column_type == DataValue::DOUBLE_VALUE)
            {
              column_name = column_name.substr(3);
              double value = 0.0;
              Sql::extractValue<double>(&value, stmt, i);          
              feature.setMetaValue(column_name, value); 
              continue;
            } else if (column_type == DataValue::STRING_LIST)
            {
              column_name = column_name.substr(4);
              String value; 
              Sql::extractValue<String>(&value, stmt, i);

              StringList sl;
              // cut off "[" and "]""
              value = value.chop(1);
              value = value.substr(1);
              value.split(", ", sl);
              feature.setMetaValue(column_name, sl);
              continue;
            } else if (column_type == DataValue::INT_LIST)
            {
              column_name = column_name.substr(4);
              String value; //IntList value;
              Sql::extractValue<String>(&value, stmt, i); //IntList
              value = value.chop(1);
              value = value.substr(1);
              std::vector<String> value_list;
              IntList il = ListUtils::create<int>(value, ',');
              feature.setMetaValue(column_name, il);
              continue;
            } else if (column_type == DataValue::DOUBLE_LIST)
            {
              column_name = column_name.substr(4);
              String value; //DoubleList value;
              Sql::extractValue<String>(&value, stmt, i); //DoubleList
              value = value.chop(1);
              value = value.substr(1);          
              DoubleList dl = ListUtils::create<double>(value, ',');
              feature.setMetaValue(column_name, dl);
              continue;
            } else if (column_type == DataValue::EMPTY_VALUE)
            {
              String value;
              Sql::extractValue<String>(&value, stmt, i);
              continue;
            }
          }
        
          // add offset of one for id column to index
          col_nr = cols_features + 1; //56 + 1

          double min_mz = 0.0;
          Sql::extractValue<double>(&min_mz, stmt, (col_nr + 0));
          double min_rt = 0.0;
          Sql::extractValue<double>(&min_rt, stmt, (col_nr + 1));
          double max_mz = 0.0;
          Sql::extractValue<double>(&max_mz, stmt, (col_nr + 2));
          double max_rt = 0.0;
          Sql::extractValue<double>(&max_rt, stmt, (col_nr + 3));

          ConvexHull2D hull;
          hull.addPoint({min_mz, min_rt});
          hull.addPoint({max_mz, max_rt});
          feature.getConvexHulls().push_back(hull);

        } 
        else if (bbox_idx > 0) // add new bb to existing feature
        {
          std::cout << "\n bbox_idx = " << bbox_idx << std::endl;

          int col_nr = cols_features + 1; //56 + 1

          double min_mz = 0.0;
          Sql::extractValue<double>(&min_mz, stmt, (col_nr + 0));
          double min_rt = 0.0;
          Sql::extractValue<double>(&min_rt, stmt, (col_nr + 1));
          double max_mz = 0.0;
          Sql::extractValue<double>(&max_mz, stmt, (col_nr + 2));
          double max_rt = 0.0;
          Sql::extractValue<double>(&max_rt, stmt, (col_nr + 3));

          ConvexHull2D hull;
          hull.addPoint({min_mz, min_rt});
          hull.addPoint({max_mz, max_rt});
          feature.getConvexHulls().push_back(hull);
        }

        sqlite3_step(stmt);
        if (sqlite3_column_type( stmt, 0 ) == SQLITE_NULL) feature_map.push_back(feature); 

        /// save feature in FeatureMap object
        //feature_map.ensureUniqueId();
        //sqlite3_step(stmt);
      }
      sqlite3_finalize(stmt);

      // return FeatureMap 
      return feature_map;

    }
    */



    // features
    if (features_switch)
    {
      /// 1. get feature data from database
      String sql = "SELECT * FROM  FEATURES_TABLE LEFT JOIN FEATURES_TABLE_BOUNDINGBOX ON FEATURES_TABLE.id = FEATURES_TABLE_BOUNDINGBOX.ref_id;";
      SqliteConnector::prepareStatement(db, &stmt, sql);
      //SqliteConnector::executePreparedStatement(db, &stmt, sql);
      sqlite3_step(stmt);

      FeatureMap feature_map;
      bool feature_switch = false;

      Feature feature_old;

      while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
      {
        int bbox_col = cols_features + 1 + 4; //56 in features + 1 offset id + 4 position of bbox_idx column
        int bbox_idx = 0;
        Sql::extractValue<int>(&bbox_idx, stmt, bbox_col);

        if (bbox_idx == 0)
        {
          if (feature_switch == true)
          {
            feature_map.push_back(feature_old);
          }
          feature_switch = true;

          //std::cout << "\n bbox_idx = " << bbox_idx << std::endl;
          Feature current_feature;
          
          // set feature parameters
          String id_s;
          long id = 0;
          // extract as String TODO
          Sql::extractValue<String>(&id_s, stmt, 0);
          id = stol(id_s);
          double rt = 0.0;
          Sql::extractValue<double>(&rt, stmt, 1);
          double mz = 0.0;
          Sql::extractValue<double>(&mz, stmt, 2);
          double intensity = 0.0;
          Sql::extractValue<double>(&intensity, stmt, 3);
          int charge = 0;
          Sql::extractValue<int>(&charge, stmt, 4);
          double quality = 0.0;
          Sql::extractValue<double>(&quality, stmt, 5);

          // get values id, RT, MZ, Intensity, Charge, Quality
          // save SQL column elements as feature
          current_feature.setUniqueId(id);
          current_feature.setRT(rt);
          current_feature.setMZ(mz);
          current_feature.setIntensity(intensity);
          current_feature.setCharge(charge);
          current_feature.setOverallQuality(quality);

          // index to account for offsets in joined tables
          int col_nr = 6;
          // save userparam columns
          for (int i = col_nr; i < cols_features ; ++i)
          {
            String column_name = sqlite3_column_name(stmt, i);
            int column_type = getColumnDatatype(column_name);

            //std::cout << "\n" << column_name << std::endl;

            if (column_type == DataValue::STRING_VALUE)
            {
              column_name = column_name.substr(3);
              String value;
              Sql::extractValue<String>(&value, stmt, i);
              current_feature.setMetaValue(column_name, value); 
              continue;
            } else if (column_type == DataValue::INT_VALUE)
            {
              column_name = column_name.substr(3);
              int value = 0;
              Sql::extractValue<int>(&value, stmt, i);
              current_feature.setMetaValue(column_name, value); 
              continue;
            } else if (column_type == DataValue::DOUBLE_VALUE)
            {
              column_name = column_name.substr(3);
              double value = 0.0;
              Sql::extractValue<double>(&value, stmt, i);          
              current_feature.setMetaValue(column_name, value); 
              continue;
            } else if (column_type == DataValue::STRING_LIST)
            {
              column_name = column_name.substr(4);
              String value; 
              Sql::extractValue<String>(&value, stmt, i);

              StringList sl;
              // cut off "[" and "]""
              value = value.chop(1);
              value = value.substr(1);
              value.split(", ", sl);
              current_feature.setMetaValue(column_name, sl);
              continue;
            } else if (column_type == DataValue::INT_LIST)
            {
              column_name = column_name.substr(4);
              String value; //IntList value;
              Sql::extractValue<String>(&value, stmt, i); //IntList
              value = value.chop(1);
              value = value.substr(1);
              std::vector<String> value_list;
              IntList il = ListUtils::create<int>(value, ',');
              current_feature.setMetaValue(column_name, il);
              continue;
            } else if (column_type == DataValue::DOUBLE_LIST)
            {
              column_name = column_name.substr(4);
              String value; //DoubleList value;
              Sql::extractValue<String>(&value, stmt, i); //DoubleList
              value = value.chop(1);
              value = value.substr(1);          
              DoubleList dl = ListUtils::create<double>(value, ',');
              current_feature.setMetaValue(column_name, dl);
              continue;
            } else if (column_type == DataValue::EMPTY_VALUE)
            {
              String value;
              Sql::extractValue<String>(&value, stmt, i);
              continue;
            }
          }
        
          // add offset of one for id column to index
          col_nr = cols_features + 1; //56 + 1

          double min_mz = 0.0;
          Sql::extractValue<double>(&min_mz, stmt, (col_nr + 0));
          double min_rt = 0.0;
          Sql::extractValue<double>(&min_rt, stmt, (col_nr + 1));
          double max_mz = 0.0;
          Sql::extractValue<double>(&max_mz, stmt, (col_nr + 2));
          double max_rt = 0.0;
          Sql::extractValue<double>(&max_rt, stmt, (col_nr + 3));

          std::cout << "############################################" << std::endl;
          std::cout << max_rt << std::endl;
          std::cout << "############################################" << std::endl;

          ConvexHull2D hull;
          hull.addPoint({min_mz, min_rt});
          hull.addPoint({max_mz, max_rt});
          current_feature.getConvexHulls().push_back(hull);

          feature_old = current_feature;

          sqlite3_step(stmt);

        }
        else if (bbox_idx > 0)
        {
          std::cout << "\n bbox_idx = " << bbox_idx << std::endl;

          int col_nr = cols_features + 1; //56 + 1

          double min_mz = 0.0;
          Sql::extractValue<double>(&min_mz, stmt, (col_nr + 0));
          double min_rt = 0.0;
          Sql::extractValue<double>(&min_rt, stmt, (col_nr + 1));
          double max_mz = 0.0;
          Sql::extractValue<double>(&max_mz, stmt, (col_nr + 2));
          double max_rt = 0.0;
          Sql::extractValue<double>(&max_rt, stmt, (col_nr + 3));

          ConvexHull2D hull;
          hull.addPoint({min_mz, min_rt});
          hull.addPoint({max_mz, max_rt});
          feature_old.getConvexHulls().push_back(hull);

          sqlite3_step(stmt);

        }
        /// save feature in FeatureMap object
        //feature_map.ensureUniqueId();
        //sqlite3_step(stmt);
      }
      feature_map.push_back(feature_old);

      sqlite3_finalize(stmt);

      // return FeatureMap 
      return feature_map;

    }
  






















  /*
    // subordinates
    if (subordinates_switch)
    {
      sql = "SELECT * FROM  FEATURES_TABLE LEFT JOIN FEATURES_SUBORDINATES ON FEATURES_TABLE.id = FEATURES_SUBORDINATES.ref_id LEFT JOIN SUBORDINATES_TABLE_BOUNDINGBOX ON FEATURES_SUBORDINATES.id = SUBORDINATES_TABLE_BOUNDINGBOX.id ORDER BY FEATURES_TABLE.ID;";
      SqliteConnector::prepareStatement(db, &stmt, sql);
      sqlite3_step(stmt);

      int counter = 0;
      int column_nr = 0;

      std::vector<Feature> subordinates;
      long s_id_old = 0;
      int feat_id_cnt = 1;
      Feature feature_old;
      Feature subordinate_old;

      while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
      {
        if (counter == 0)
        {
          for (int i = 0; i < cols_features_join_subordinates_join_bbox ; ++i)
          {
            String column_name = sqlite3_column_name(stmt, i);
            std::cout << i << "th column with name: " << column_name << std::endl; 
          }
        }
        ++counter;
        //std::cout << "number of lines " << counter << std::endl; 


        Feature feature;
        Feature subordinate;

        // set feature ID
        String feat_id;
        long f_id = 0;
        Sql::extractValue<String>(&feat_id, stmt, 0);
        f_id = stol(feat_id);
        
        if (feat_id_cnt != 0)
        {
          feature_old = f_id;
          --feat_id_cnt;
          feature.setUniqueId(f_id);
        } 

        if ((feature_old != f_id) && (feat_id_cnt == 0))
        {

        }




        // set subordinate ID
        String sub_id;
        long s_id = 0;
        Sql::extractValue<String>(&sub_id, stmt, cols_features); // 
        s_id = stol(sub_id);
        

        // get boundingbox index
        int bbox_col = cols_features_join_subordinates_join_bbox - 1; // - 1 to address last column
        int bbox_idx = 0; // set start idx of boundingbox
        Sql::extractValue<int>(&bbox_idx, stmt, bbox_col);

        // return sub_idx
        int sub_idx = 0;
        Sql::extractValue<int>(&sub_idx, stmt, 57);



        // check if sub_idx is 0 => new subordinate
        // if sub_idx is not 0 => old subordinate, add bbox to current subordinate
        if (sub_idx == 0)
        {
          std::cout << "s_id_old \t f_id \t s_id" << std::endl;
          std::cout << s_id_old << "\t" << f_id << "\t" << s_id << std::endl;

          column_nr = cols_features + 1 + 1;// size features + column SUB_IDX + column REF_ID
          double rt = 0.0;
          Sql::extractValue<double>(&rt, stmt, column_nr + 1);
          double mz = 0.0;
          Sql::extractValue<double>(&mz, stmt, column_nr + 2);
          double intensity = 0.0;
          Sql::extractValue<double>(&intensity, stmt, column_nr + 3);
          int charge = 0;
          Sql::extractValue<int>(&charge, stmt, column_nr + 4);
          double quality = 0.0;
          Sql::extractValue<double>(&quality, stmt, column_nr + 5);
          subordinate.setUniqueId(s_id);
          subordinate.setRT(rt);
          subordinate.setMZ(mz);
          subordinate.setIntensity(intensity);
          subordinate.setCharge(charge);
          subordinate.setOverallQuality(quality);
          // userparams
          column_nr = cols_features + 5;  // +5 = subordinate params
          for (int i = column_nr; i < cols_features + cols_subordinates - 1 ; ++i) // subordinate userparams - 1 = index of last element in cols_subordinates
          {
            String column_name = sqlite3_column_name(stmt, i);
            int column_type = getColumnDatatype(column_name);

            //std::cout << column_name << std::endl;

            if (column_type == DataValue::STRING_VALUE)
            {
              column_name = column_name.substr(3);
              String value;
              Sql::extractValue<String>(&value, stmt, i);
              subordinate.setMetaValue(column_name, value); 
              continue;
            } else if (column_type == DataValue::INT_VALUE)
            {
              column_name = column_name.substr(3);
              int value = 0;
              Sql::extractValue<int>(&value, stmt, i);
              subordinate.setMetaValue(column_name, value); 
              continue;
            } else if (column_type == DataValue::DOUBLE_VALUE)
            {
              column_name = column_name.substr(3);
              double value = 0.0;
              Sql::extractValue<double>(&value, stmt, i);          
              subordinate.setMetaValue(column_name, value); 
              continue;
            } else if (column_type == DataValue::STRING_LIST)
            {
              column_name = column_name.substr(4);
              String value; 
              Sql::extractValue<String>(&value, stmt, i);

              StringList sl;
              // cut off "[" and "]""
              value = value.chop(1);
              value = value.substr(1);
              value.split(", ", sl);
              subordinate.setMetaValue(column_name, sl);
              continue;
            } else if (column_type == DataValue::INT_LIST)
            {
              column_name = column_name.substr(4);
              String value; //IntList value;
              Sql::extractValue<String>(&value, stmt, i); //IntList
              value = value.chop(1);
              value = value.substr(1);
              std::vector<String> value_list;
              IntList il = ListUtils::create<int>(value, ',');
              subordinate.setMetaValue(column_name, il);
              continue;
            } else if (column_type == DataValue::DOUBLE_LIST)
            {
              column_name = column_name.substr(4);
              String value; //DoubleList value;
              Sql::extractValue<String>(&value, stmt, i); //DoubleList
              value = value.chop(1);
              value = value.substr(1);          
              DoubleList dl = ListUtils::create<double>(value, ',');
              subordinate.setMetaValue(column_name, dl);
              continue;
            } else if (column_type == DataValue::EMPTY_VALUE)
            {
              String value;
              Sql::extractValue<String>(&value, stmt, i);
              continue;
            }
          }
          // current_subordinate add bbox
          column_nr = cols_features + cols_subordinates + 1; //+ 1 = REF_ID
          double min_mz = 0.0;
          Sql::extractValue<double>(&min_mz, stmt, (column_nr + 1));
          double min_rt = 0.0;
          Sql::extractValue<double>(&min_rt, stmt, (column_nr + 2));
          double max_mz = 0.0;
          Sql::extractValue<double>(&max_mz, stmt, (column_nr + 3));
          double max_rt = 0.0;
          Sql::extractValue<double>(&max_rt, stmt, (column_nr + 4));

          //std::cout << min_mz << "\t" << min_rt << "\t" << max_mz << "\t" << max_rt << std::endl;

          ConvexHull2D hull;
          hull.addPoint({min_mz, min_rt});
          hull.addPoint({max_mz, max_rt});
          subordinate.getConvexHulls().push_back(hull);
          subordinates.push_back(subordinate);
          
        } else if (sub_idx != 0) 
        // check if subordinate == subordinate_old => add bbox to feature
        // else => add bbox to current_feature
        {
          // same subordinates
          if (s_id == s_id_old)
          {
            // add convex hull to previous subordinate
            std::cout << "s_id == s_id_old" << std::endl;
            std::cout << s_id << "\t" << s_id_old << std::endl;

            column_nr = cols_features + cols_subordinates + 1; //+ 1 = REF_ID
            double min_mz = 0.0;
            Sql::extractValue<double>(&min_mz, stmt, (column_nr + 1));
            double min_rt = 0.0;
            Sql::extractValue<double>(&min_rt, stmt, (column_nr + 2));
            double max_mz = 0.0;
            Sql::extractValue<double>(&max_mz, stmt, (column_nr + 3));
            double max_rt = 0.0;
            Sql::extractValue<double>(&max_rt, stmt, (column_nr + 4));

            //std::cout << min_mz << "\t" << min_rt << "\t" << max_mz << "\t" << max_rt << std::endl;

            ConvexHull2D hull;
            hull.addPoint({min_mz, min_rt});
            hull.addPoint({max_mz, max_rt});
            subordinate_old.getConvexHulls().push_back(hull);
            subordinate = subordinate_old;
            subordinates.push_back(subordinate);

          } else if (s_id != s_id_old)
          // different subordinates
          {
            feature_map.push_back(feature);
            subordinates.clear();
            // add convex hull to current subordinate
            std::cout << "s_id != s_id_old" << std::endl;
            std::cout << s_id << "\t" << s_id_old << std::endl;

            column_nr = cols_features + 1 + 1;// size features + column SUB_IDX + column REF_ID
            double rt = 0.0;
            Sql::extractValue<double>(&rt, stmt, column_nr + 1);
            double mz = 0.0;
            Sql::extractValue<double>(&mz, stmt, column_nr + 2);
            double intensity = 0.0;
            Sql::extractValue<double>(&intensity, stmt, column_nr + 3);
            int charge = 0;
            Sql::extractValue<int>(&charge, stmt, column_nr + 4);
            double quality = 0.0;
            Sql::extractValue<double>(&quality, stmt, column_nr + 5);
            subordinate.setUniqueId(s_id);
            subordinate.setRT(rt);
            subordinate.setMZ(mz);
            subordinate.setIntensity(intensity);
            subordinate.setCharge(charge);
            subordinate.setOverallQuality(quality);
            // userparams
            column_nr = cols_features + 5;  // +5 = subordinate params
            for (int i = column_nr; i < cols_features + cols_subordinates - 1 ; ++i) // subordinate userparams - 1 = index of last element in cols_subordinates
            {
              String column_name = sqlite3_column_name(stmt, i);
              int column_type = getColumnDatatype(column_name);

              //std::cout << column_name << std::endl;

              if (column_type == DataValue::STRING_VALUE)
              {
                column_name = column_name.substr(3);
                String value;
                Sql::extractValue<String>(&value, stmt, i);
                subordinate.setMetaValue(column_name, value); 
                continue;
              } else if (column_type == DataValue::INT_VALUE)
              {
                column_name = column_name.substr(3);
                int value = 0;
                Sql::extractValue<int>(&value, stmt, i);
                subordinate.setMetaValue(column_name, value); 
                continue;
              } else if (column_type == DataValue::DOUBLE_VALUE)
              {
                column_name = column_name.substr(3);
                double value = 0.0;
                Sql::extractValue<double>(&value, stmt, i);          
                subordinate.setMetaValue(column_name, value); 
                continue;
              } else if (column_type == DataValue::STRING_LIST)
              {
                column_name = column_name.substr(4);
                String value; 
                Sql::extractValue<String>(&value, stmt, i);

                StringList sl;
                // cut off "[" and "]""
                value = value.chop(1);
                value = value.substr(1);
                value.split(", ", sl);
                subordinate.setMetaValue(column_name, sl);
                continue;
              } else if (column_type == DataValue::INT_LIST)
              {
                column_name = column_name.substr(4);
                String value; //IntList value;
                Sql::extractValue<String>(&value, stmt, i); //IntList
                value = value.chop(1);
                value = value.substr(1);
                std::vector<String> value_list;
                IntList il = ListUtils::create<int>(value, ',');
                subordinate.setMetaValue(column_name, il);
                continue;
              } else if (column_type == DataValue::DOUBLE_LIST)
              {
                column_name = column_name.substr(4);
                String value; //DoubleList value;
                Sql::extractValue<String>(&value, stmt, i); //DoubleList
                value = value.chop(1);
                value = value.substr(1);          
                DoubleList dl = ListUtils::create<double>(value, ',');
                subordinate.setMetaValue(column_name, dl);
                continue;
              } else if (column_type == DataValue::EMPTY_VALUE)
              {
                String value;
                Sql::extractValue<String>(&value, stmt, i);
                continue;
              }
            }
            // current_subordinate add bbox
            column_nr = cols_features + cols_subordinates + 1; //+ 1 = REF_ID
            double min_mz = 0.0;
            Sql::extractValue<double>(&min_mz, stmt, (column_nr + 1));
            double min_rt = 0.0;
            Sql::extractValue<double>(&min_rt, stmt, (column_nr + 2));
            double max_mz = 0.0;
            Sql::extractValue<double>(&max_mz, stmt, (column_nr + 3));
            double max_rt = 0.0;
            Sql::extractValue<double>(&max_rt, stmt, (column_nr + 4));

            std::cout << min_mz << "\t" << min_rt << "\t" << max_mz << "\t" << max_rt << std::endl;

            ConvexHull2D hull;
            hull.addPoint({min_mz, min_rt});
            hull.addPoint({max_mz, max_rt});
            subordinate.getConvexHulls().push_back(hull);
            subordinates.push_back(subordinate);

          }
        }

        // set s_id_old
        Sql::extractValue<String>(&sub_id, stmt, cols_features); // 
        s_id_old = stol(sub_id);
        subordinate_old = subordinate;

        sqlite3_step(stmt);
      
      }
      sqlite3_finalize(stmt);

      // return FeatureMap 
      return feature_map;
    }
  */
  }
} // namespace OpenMS





























  
  
  

  
















































































  
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  //                                      old version of working code "multi-loop"                                   //
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
  /*
    if (features_switch)
    {
      /// 1. get feature data from database
      /// traverse across features
      // get feature table entries
      String sql = "SELECT * FROM FEATURES_TABLE;";
      SqliteConnector::prepareStatement(db, &stmt, sql);
      //SqliteConnector::executePreparedStatement(db, &stmt, sql);
      sqlite3_step(stmt);
      
      while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
      {
        Feature current_feature;

        // set feature parameters
        String id_s;
        long id = 0;
        // extract as String TODO
        Sql::extractValue<String>(&id_s, stmt, 0);
        id = stol(id_s);
        double rt = 0.0;
        Sql::extractValue<double>(&rt, stmt, 1);
        double mz = 0.0;
        Sql::extractValue<double>(&mz, stmt, 2);
        double intensity = 0.0;
        Sql::extractValue<double>(&intensity, stmt, 3);
        int charge = 0;
        Sql::extractValue<int>(&charge, stmt, 4);
        double quality = 0.0;
        Sql::extractValue<double>(&quality, stmt, 5);


        // get values id, RT, MZ, Intensity, Charge, Quality
        // save SQL column elements as feature
        current_feature.setUniqueId(id);
        current_feature.setRT(rt);
        current_feature.setMZ(mz);
        current_feature.setIntensity(intensity);
        current_feature.setCharge(charge);
        current_feature.setOverallQuality(quality);

        // save userparam column names
        // read colum names, infer DataValue and write data to current_feature
        // with setMetaValue(value, datatype) DataValue::DataType
        // access number of columns and infer type
        int cols = sqlite3_column_count(stmt);
        String column_name;
        int column_type = 0;

        // spare first values up to index 5 of feature parameters
        // parse userparameters with respective DataValue::DataType
        // TODO: Integer values incorrectly typed as String
        // because of read out error
        for (int i = 6; i < cols ; ++i)
        {
          column_name = sqlite3_column_name(stmt, i);
          column_type = getColumnDatatype(column_name);
          if (column_type == DataValue::STRING_VALUE)
          {
            column_name = column_name.substr(3);
            String value;
            Sql::extractValue<String>(&value, stmt, i);
            current_feature.setMetaValue(column_name, value); 
            continue;
          } else if (column_type == DataValue::INT_VALUE)
          {
            column_name = column_name.substr(3);
            int value = 0;
            Sql::extractValue<int>(&value, stmt, i);
            current_feature.setMetaValue(column_name, value); 
            continue;
          } else if (column_type == DataValue::DOUBLE_VALUE)
          {
            column_name = column_name.substr(3);
            double value = 0.0;
            Sql::extractValue<double>(&value, stmt, i);          
            current_feature.setMetaValue(column_name, value); 
            continue;
          } else if (column_type == DataValue::STRING_LIST)
          {
            column_name = column_name.substr(4);
            String value; 
            Sql::extractValue<String>(&value, stmt, i);

            StringList sl;
            // cut off "[" and "]""
            value = value.chop(1);
            value = value.substr(1);
            value.split(", ", sl);
            current_feature.setMetaValue(column_name, sl);
            continue;
          } else if (column_type == DataValue::INT_LIST)
          {
            column_name = column_name.substr(4);
            String value; //IntList value;
            Sql::extractValue<String>(&value, stmt, i); //IntList
            value = value.chop(1);
            value = value.substr(1);
            std::vector<String> value_list;
            IntList il = ListUtils::create<int>(value, ',');
            current_feature.setMetaValue(column_name, il);
            continue;
          } else if (column_type == DataValue::DOUBLE_LIST)
          {
            column_name = column_name.substr(4);
            String value; //DoubleList value;
            Sql::extractValue<String>(&value, stmt, i); //DoubleList
            value = value.chop(1);
            value = value.substr(1);          
            DoubleList dl = ListUtils::create<double>(value, ',');
            current_feature.setMetaValue(column_name, dl);
            continue;
          } else if (column_type == DataValue::EMPTY_VALUE)
          {
            String value;
            Sql::extractValue<String>(&value, stmt, i);
            continue;
          }
        }


        /// save feature in FeatureMap object
        feature_map.push_back(current_feature);
        //feature_map.ensureUniqueId();
        sqlite3_step(stmt);
      }
      sqlite3_finalize(stmt);
    }



    /// 2. prepare subordinate data from database for later insertion

    /// new version with sqlite database query solution
    // query all lines with id of FEATURES_TABLE and ref_id FEATURES_SUBORDINATES being identical
    // store in map with pair int feature
    // map <int, <vector> features>     std::map<long, std::vector<Feature>> id_subs;
    // use insert 
    
    // query SELECT * FROM FEATURES_SUBORDINATES INNER JOIN FEATURES_TABLE ON FEATURES_SUBORDINATES.REF_ID = FEATURES_TABLE.ID

    if (subordinates_switch)
    {
      /// get subordinate ref_id vector and subordinate vector
      String sql = "SELECT * FROM FEATURES_SUBORDINATES INNER JOIN FEATURES_TABLE ON FEATURES_SUBORDINATES.REF_ID = FEATURES_TABLE.ID;";
      SqliteConnector::prepareStatement(db, &stmt, sql);
      //SqliteConnector::executePreparedStatement(db, &stmt, sql);
      sqlite3_step(stmt);

      // container to save subordinate construct
      std::map<long, std::vector< std::pair<int, Feature>>> id_subs;

      while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
      {
        Feature current_sub_feature;

        String id_s;
        long id = 0;
        Sql::extractValue<String>(&id_s, stmt, 0);
        id = stol(id_s);

        int sub_idx = 0;
        Sql::extractValue<int>(&sub_idx, stmt, 1);

        String ref_id;
        long r_id = 0;
        Sql::extractValue<String>(&ref_id, stmt, 2);
        r_id = stol(ref_id);

        double rt = 0.0;
        Sql::extractValue<double>(&rt, stmt, 3);

        double mz = 0.0;
        Sql::extractValue<double>(&mz, stmt, 4);

        double intensity = 0.0;
        Sql::extractValue<double>(&intensity, stmt, 5);

        int charge = 0;
        Sql::extractValue<int>(&charge, stmt, 6);

        double quality = 0.0;
        Sql::extractValue<double>(&quality, stmt, 7);

        current_sub_feature.setUniqueId(id);
        current_sub_feature.setRT(rt);
        current_sub_feature.setMZ(mz);
        current_sub_feature.setIntensity(intensity);
        current_sub_feature.setCharge(charge);
        current_sub_feature.setOverallQuality(quality);
        
        // save userparam column names
        // read colum names, infer DataValue and write data to current_feature
        // with setMetaValue(value, datatype) DataValue::DataType

        // access number of columns and infer DataType
        int cols = sqlite3_column_count(stmt);
        
        String column_name;
        int column_type = 0;

        // spare first seven values with feature parameters
        // parse values with respective DataValue::DataType
        for (int i = 8; i < cols ; ++i)
        {
          column_name = sqlite3_column_name(stmt, i);
          column_type = getColumnDatatype(column_name);
          if (column_type == DataValue::STRING_VALUE)
          {
            column_name = column_name.substr(3);
            String value;
            Sql::extractValue<String>(&value, stmt, i);
            current_sub_feature.setMetaValue(column_name, value); 
            continue;
          } else if (column_type == DataValue::INT_VALUE)
          {
            column_name = column_name.substr(3);
            int value = 0;
            Sql::extractValue<int>(&value, stmt, i);
            current_sub_feature.setMetaValue(column_name, value); 
            continue;
          } else if (column_type == DataValue::DOUBLE_VALUE)
          {
            column_name = column_name.substr(3);
            double value = 0.0;
            Sql::extractValue<double>(&value, stmt, i);          
            current_sub_feature.setMetaValue(column_name, value); 
            continue;
          } else if (column_type == DataValue::STRING_LIST)
          {
            column_name = column_name.substr(4);
            String value; 
            Sql::extractValue<String>(&value, stmt, i);
            StringList sl;
            // cut off "[" and "]""
            value = value.chop(1);
            value = value.substr(1);
            value.split(", ", sl);
            current_sub_feature.setMetaValue(column_name, sl);
            continue;
          } else if (column_type == DataValue::INT_LIST)
          {
            column_name = column_name.substr(4);
            String value; //IntList value;
            Sql::extractValue<String>(&value, stmt, i); //IntList
            value = value.chop(1);
            value = value.substr(1);
            std::vector<String> value_list;
            IntList il = ListUtils::create<int>(value, ',');
            current_sub_feature.setMetaValue(column_name, il);
            continue;
          } else if (column_type == DataValue::DOUBLE_LIST)
          {
            column_name = column_name.substr(4);
            String value; //DoubleList value;
            Sql::extractValue<String>(&value, stmt, i); //DoubleList
            value = value.chop(1);
            value = value.substr(1);          
            DoubleList dl = ListUtils::create<double>(value, ',');
            current_sub_feature.setMetaValue(column_name, dl);
            continue;
          } else if (column_type == DataValue::EMPTY_VALUE)
          {
            String value;
            Sql::extractValue<String>(&value, stmt, i);
            continue;
          }
        }

        std::pair<int, Feature> id_subordinate = std::make_pair(sub_idx, current_sub_feature);
        id_subs[r_id].push_back(id_subordinate);

        /// iterate across table lines
        sqlite3_step(stmt);
      }
      sqlite3_finalize(stmt);

      /// save subordinates to feature_map
      /// loop across features in featuremap
      for (Feature& feature : feature_map)
      {
        /// get feature id 
        long id = static_cast<int64_t>(feature.getUniqueId() & ~(1ULL << 63));
        
        /// find feature id in map as key
        auto idx_sub = id_subs.find(id);

        //std::vector<int, Feature> test;
        auto pairs = idx_sub->second;

        /// return number of keys
        int vec_size = pairs.size();

        /// create subordinate vector with size according to amount of pairs
        std::vector<Feature> subordinates(vec_size);

        for (int pos = 0; pos != vec_size; ++pos)
        {
          int current_sub_pos = pairs[pos].first;
          Feature current_sub = pairs[pos].second;
          subordinates[current_sub_pos] = current_sub;
        }
      
        /// set subordinates
        feature.setSubordinates(subordinates);
      }
    }







    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                                     3. add boundingbox to FeatureMap                                                 //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if (features_bbox_switch)
    {
      // 1. features
      // container to save bounding box 
      // save ID, bb_idx, tuple of 
      std::map <long, std::vector <std::pair< int, std::tuple < double, double, double, double>>>> feat_id_bboxes;

      // access correct feature
      String sql = "SELECT * FROM FEATURES_TABLE_BOUNDINGBOX INNER JOIN FEATURES_TABLE ON FEATURES_TABLE_BOUNDINGBOX.REF_ID = FEATURES_TABLE.ID;";
      SqliteConnector::prepareStatement(db, &stmt, sql);
      //SqliteConnector::executePreparedStatement(db, &stmt, sql);
      sqlite3_step(stmt);

      while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
      {
        String id_s;
        long id;
        // extract as String
        Sql::extractValue<String>(&id_s, stmt, 0);
        id = stol(id_s);
        double min_mz;
        Sql::extractValue<double>(&min_mz, stmt, 1);
        double min_rt;
        Sql::extractValue<double>(&min_rt, stmt, 2);
        double max_mz;
        Sql::extractValue<double>(&max_mz, stmt, 3);
        double max_rt;
        Sql::extractValue<double>(&max_rt, stmt, 4);
        int bb_idx;
        Sql::extractValue<int>(&bb_idx, stmt, 5);

        std::tuple<double, double, double, double> current_bbox = std::make_tuple(min_mz, min_rt, max_mz, max_rt);
        std::pair<int, std::tuple<double, double, double, double>> idx_bbox = std::make_pair(bb_idx, current_bbox);
        feat_id_bboxes[id].push_back(idx_bbox);

        sqlite3_step(stmt);
      }
      sqlite3_finalize(stmt);


      /// save convexhull points to feature_map
      /// loop across features in featuremap
      for (Feature& feature : feature_map)
      {
        /// get feature id 
        long id = static_cast<int64_t>(feature.getUniqueId() & ~(1ULL << 63));
        
        /// find feature id in boundingbox map as key
        auto idx_box = feat_id_bboxes.find(id);

        auto pairs = idx_box->second;


        /// return number of keys
        int vec_size = pairs.size();

        for (int pos = 0; pos != vec_size; ++pos)
        {
          double min_mz = std::get<0>(pairs[pos].second);
          double min_rt = std::get<1>(pairs[pos].second);
          double max_mz = std::get<2>(pairs[pos].second);
          double max_rt = std::get<3>(pairs[pos].second);

          ConvexHull2D hull;
          hull.addPoint({min_mz, min_rt});
          hull.addPoint({max_mz, max_rt});
          feature.getConvexHulls().push_back(hull);
        }
      }
      // clear memory of feat_id_bbox
      feat_id_bboxes.clear();
    }

    
    if (subordinates_bbox_switch)
    {
      // 2. subordinates  
      String sql = "SELECT * FROM SUBORDINATES_TABLE_BOUNDINGBOX INNER JOIN FEATURES_TABLE ON SUBORDINATES_TABLE_BOUNDINGBOX.REF_ID = FEATURES_TABLE.ID";
      SqliteConnector::prepareStatement(db, &stmt, sql);
      //SqliteConnector::executePreparedStatement(db, &stmt, sql);
      sqlite3_step(stmt);

      // container to save bounding box 
      std::map <String, std::vector <std::pair< int, std::tuple < double, double, double, double>>>> sub_id_bboxes;
      //std::multimap <long, std::map <long, std::vector <std::pair <int, std::tuple <double, double, double, double>>>>> sub_id_bboxes;

      while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
      {
        String sub_id;
        // extract as String
        Sql::extractValue<String>(&sub_id, stmt, 0);

        String feat_id;
        // extract as String
        Sql::extractValue<String>(&feat_id, stmt, 1);

        String feat_sub_id = feat_id + sub_id;

        double min_mz;
        Sql::extractValue<double>(&min_mz, stmt, 2);
        double min_rt;
        Sql::extractValue<double>(&min_rt, stmt, 3);
        double max_mz;
        Sql::extractValue<double>(&max_mz, stmt, 4);
        double max_rt;
        Sql::extractValue<double>(&max_rt, stmt, 5);
        int bb_idx;
        Sql::extractValue<int>(&bb_idx, stmt, 6);

        /// save current row as map of map of vectors
        /// traversing hierarchies of features, subordinates and convexhull points respectively
        std::tuple<double, double, double, double> current_bbox = std::make_tuple(min_mz, min_rt, max_mz, max_rt);
        std::pair<int, std::tuple<double, double, double, double>> idx_bbox = std::make_pair(bb_idx, current_bbox);
        sub_id_bboxes[feat_sub_id].push_back(idx_bbox);

        sqlite3_step(stmt);
      }
      sqlite3_finalize(stmt);


      for (Feature& feature : feature_map)
      {
        /// get feature id 
        long feat_id = static_cast<int64_t>(feature.getUniqueId() & ~(1ULL << 63));

        for (Feature& sub : feature.getSubordinates())
        {
          /// get subordinate id 
          long sub_id = static_cast<int64_t>(sub.getUniqueId() & ~(1ULL << 63));

          String feature_id(feat_id);
          String subordinate_id(sub_id);

          String feat_sub = feature_id + subordinate_id;

          std::cout << feat_sub << std::endl;

          /// find feature id in boundingbox map as key
          auto idx_box = sub_id_bboxes.find(feat_sub);

          auto pairs = idx_box->second;

          /// return number of keys
          int vec_size = pairs.size();
          
          for (int pos = 0; pos != vec_size; ++pos)
          {
            double min_mz = std::get<0>(pairs[pos].second);
            double min_rt = std::get<1>(pairs[pos].second);
            double max_mz = std::get<2>(pairs[pos].second);
            double max_rt = std::get<3>(pairs[pos].second);

            ConvexHull2D hull;
            hull.addPoint({min_mz, min_rt});
            hull.addPoint({max_mz, max_rt});
            sub.getConvexHulls().push_back(hull);
          }
        }
      }
      // clear memory of feat_id_bbox
      sub_id_bboxes.clear();
    }



    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //                                                     4. add dataprocessing to FeatureMap                                              //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if (dataprocessing_switch)
    {
      String sql = "SELECT * FROM FEATURES_DATAPROCESSING;";
      SqliteConnector::prepareStatement(db, &stmt, sql);
      //SqliteConnector::executePreparedStatement(db, &stmt, sql);
      sqlite3_step(stmt);

      while (sqlite3_column_type( stmt, 0 ) != SQLITE_NULL)
      {
        // set dataprocessing parameters
        String id_s;
        long id = 0;
        // extract as String
        Sql::extractValue<String>(&id_s, stmt, 0);
        id = stol(id_s);
      
        String software;
        Sql::extractValue<String>(&software, stmt, 1);
        String software_name;
        Sql::extractValue<String>(&software_name, stmt, 2);
        String data;
        Sql::extractValue<String>(&data, stmt, 3);
        String time;
        Sql::extractValue<String>(&time, stmt, 4);
        String action;
        Sql::extractValue<String>(&action, stmt, 5);

        // get values
        // id, RT, MZ, Intensity, Charge, Quality
        // save SQL column elements as feature
        feature_map.setUniqueId(id);
        DataProcessing dp;

        //software
        dp.getSoftware().setName(software);
        dp.getSoftware().setVersion(software_name);

        //time
        DateTime date_time;
        date_time.set(data + " " + time);
        dp.setCompletionTime(date_time);
        
        //actions
        std::set<DataProcessing::ProcessingAction> proc_actions;

        for (int i = 0; i < DataProcessing::SIZE_OF_PROCESSINGACTION; ++i)
        {
          if (action == DataProcessing::NamesOfProcessingAction[i])
          {
            proc_actions.insert((DataProcessing::ProcessingAction) i);
          }
        }

        dp.setProcessingActions(proc_actions);


        // access number of columns and infer DataType
        int cols = sqlite3_column_count(stmt);
        int column_type = 0;
        String column_name;

        // spare first seven values with feature parameters
        // parse values for respective DataValue::DataType
        for (int i = 5; i < cols ; ++i)
        {
          column_name = sqlite3_column_name(stmt, i);
          column_type = getColumnDatatype(column_name);
          if (column_type == DataValue::STRING_VALUE)
          {
            String value;
            column_name = column_name.substr(3);
            Sql::extractValue<String>(&value, stmt, i);
            dp.setMetaValue(column_name, value); 
            continue;
          } else if (column_type == DataValue::INT_VALUE)
          {
            int value = 0;
            column_name = column_name.substr(3);
            Sql::extractValue<int>(&value, stmt, i);
            dp.setMetaValue(column_name, value); 
            continue;
          } else if (column_type == DataValue::DOUBLE_VALUE)
          {
            double value = 0.0;
            column_name = column_name.substr(3);
            Sql::extractValue<double>(&value, stmt, i);          
            dp.setMetaValue(column_name, value); 
            continue;
          } else if (column_type == DataValue::STRING_LIST)
          {
            String value; 
            Sql::extractValue<String>(&value, stmt, i);
            column_name = column_name.substr(4);

            StringList sl;
            // cut off "[" and "]""
            value = value.chop(1);
            value = value.substr(1);
            value.split(", ", sl);
            dp.setMetaValue(column_name, sl);
            continue;
          } else if (column_type == DataValue::INT_LIST)
          {
            String value; //IntList value;
            Sql::extractValue<String>(&value, stmt, i); //IntList
            column_name = column_name.substr(4);

            value = value.chop(1);
            value = value.substr(1);
            std::vector<String> value_list;
            IntList il = ListUtils::create<int>(value, ',');
            dp.setMetaValue(column_name, il);
            continue;
          } else if (column_type == DataValue::DOUBLE_LIST)
          {
            String value; //DoubleList value;
            Sql::extractValue<String>(&value, stmt, i); //DoubleList
            column_name = column_name.substr(4);

            value = value.chop(1);
            value = value.substr(1);          
            DoubleList dl = ListUtils::create<double>(value, ',');
            dp.setMetaValue(column_name, dl);
            continue;
          } else if (column_type == DataValue::EMPTY_VALUE)
          {
            String value;
            Sql::extractValue<String>(&value, stmt, i); 
            continue;
          }
        }

        feature_map.getDataProcessing().push_back(dp);
        sqlite3_step(stmt);
      }
    }


    // close SQL database connection
    sqlite3_finalize(stmt);

    // return FeatureMap 
    return feature_map;

    fm.ensureUniqueId();



  }  
  // end of FeatureSQLFile::read

} // namespace OpenMS

*/
