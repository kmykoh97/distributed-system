package sjtu.sdic.mapreduce.core;

import com.alibaba.fastjson.JSONArray;
import com.alibaba.fastjson.JSONObject;
import io.netty.handler.codec.json.JsonObjectDecoder;
import sjtu.sdic.mapreduce.common.KeyValue;
import sjtu.sdic.mapreduce.common.Utils;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;

import static sjtu.sdic.mapreduce.common.Utils.reduceName;

/**
 * Created by Cachhe on 2019/4/19.
 */
public class Reducer {

    /**
     * 
     * 	doReduce manages one reduce task: it should read the intermediate
     * 	files for the task, sort the intermediate key/value pairs by key,
     * 	call the user-defined reduce function {@code reduceF} for each key,
     * 	and write reduceF's output to disk.
     * 	
     * 	You'll need to read one intermediate file from each map task;
     * 	{@code reduceName(jobName, m, reduceTask)} yields the file
     * 	name from map task m.
     *
     * 	Your {@code doMap()} encoded the key/value pairs in the intermediate
     * 	files, so you will need to decode them. If you used JSON, you can refer
     * 	to related docs to know how to decode.
     * 	
     *  In the original paper, sorting is optional but helpful. Here you are
     *  also required to do sorting. Lib is allowed.
     * 	
     * 	{@code reduceF()} is the application's reduce function. You should
     * 	call it once per distinct key, with a slice of all the values
     * 	for that key. {@code reduceF()} returns the reduced value for that
     * 	key.
     * 	
     * 	You should write the reduce output as JSON encoded KeyValue
     * 	objects to the file named outFile. We require you to use JSON
     * 	because that is what the merger than combines the output
     * 	from all the reduce tasks expects. There is nothing special about
     * 	JSON -- it is just the marshalling format we chose to use.
     * 	
     * 	Your code here (Part I).
     * 	
     * 	
     * @param jobName the name of the whole MapReduce job
     * @param reduceTask which reduce task this is
     * @param outFile write the output here
     * @param nMap the number of map tasks that were run ("M" in the paper)
     * @param reduceF user-defined reduce function
     */
    public static void doReduce(String jobName, int reduceTask, String outFile, int nMap, ReduceFunc reduceF) {
        // read intermediate
        List<KeyValue> arraylist = new ArrayList<>();

        for (int i = 0; i < nMap; i++) {
            String filename = reduceName(jobName, i, reduceTask);
            String content = "";
            try {
                content = new String(Files.readAllBytes(Paths.get(filename)));
//                System.out.print(content);
            } catch (IOException e) {
                e.printStackTrace();
            }
            List templist = JSONArray.parseArray(content, KeyValue.class);
            arraylist.addAll(templist);
        }

        // sort them by key
        Collections.sort(arraylist, new Comparator<KeyValue>() {
            @Override
            public int compare(KeyValue kv1, KeyValue kv2) {
                return kv1.key.compareTo(kv2.key);
            }
        });

        // call reduce functions
        JSONObject jsonObject = new JSONObject();
        List<String> sameValueList = new ArrayList<>();
        if (arraylist.size() == 0) return;
        sameValueList.add(arraylist.get(0).value);

        for (int i = 1; i < arraylist.size(); i++) {
            if (arraylist.get(i).key.equals(arraylist.get(i-1).key) == true) {
                sameValueList.add(arraylist.get(i).value);

                if (i == arraylist.size()-1) {
                    String result = reduceF.reduce(arraylist.get(i).key, sameValueList.toArray(new String[sameValueList.size()]));
                    jsonObject.put(arraylist.get(i).key, result);
                    sameValueList.clear();
                }
            } else {
                String result = reduceF.reduce(arraylist.get(i-1).key, sameValueList.toArray(new String[sameValueList.size()]));
                jsonObject.put(arraylist.get(i-1).key, result);
                sameValueList.clear();
                sameValueList.add(arraylist.get(i).value);

                if (i == arraylist.size()-1) {
                    result = reduceF.reduce(arraylist.get(i).key, sameValueList.toArray(new String[sameValueList.size()]));
                    jsonObject.put(arraylist.get(i).key, result);
                    sameValueList.clear();
                }
            }
        }

        // write output files
        FileOutputStream fileoutputstream = null;
        File fileoutput = new File(outFile);
        try {
            fileoutputstream = new FileOutputStream(fileoutput);
            String result = jsonObject.toJSONString();
            fileoutputstream.write(result.getBytes());
            fileoutputstream.close();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
