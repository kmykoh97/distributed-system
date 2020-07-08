package sjtu.sdic.mapreduce;

import org.apache.commons.io.filefilter.WildcardFileFilter;
import sjtu.sdic.mapreduce.common.KeyValue;
import sjtu.sdic.mapreduce.core.Master;
import sjtu.sdic.mapreduce.core.Worker;

import java.io.File;
import java.util.*;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Created by Cachhe on 2019/4/24.
 */
public class InvertedIndex {

    public static List<KeyValue> mapFunc(String file, String value) {
        // Your code here (Part V)
        Pattern pattern = Pattern.compile("[a-zA-Z0-9]+");
        Matcher matcher = pattern.matcher(value);
        List<KeyValue> arraylist = new ArrayList<>();

        while (matcher.find()){
            String word = matcher.group();
            arraylist.add(new KeyValue(word, file));
        }

        return arraylist;
    }

    public static String reduceFunc(String key, String[] values) {
        //  Your code here (Part V)
        String result = "";

        // there are duplicate values. So we need to use set data structure
        Set<String> hashset = new HashSet<String>();

        for (String value : values) {
            hashset.add(value);
        }

        result += Integer.toString(hashset.size()) + " ";

        for (String value : hashset) {
            result += value;
            result += ",";
        }

        result = result.substring(0, result.length() - 1);

        return result;
    }

    public static void main(String[] args) {
        if (args.length < 3) {
            System.out.println("error: see usage comments in file");
        } else if (args[0].equals("master")) {
            Master mr;

            String src = args[2];
            File file = new File(".");
            String[] files = file.list(new WildcardFileFilter(src));
            if (args[1].equals("sequential")) {
                mr = Master.sequential("iiseq", files, 3, InvertedIndex::mapFunc, InvertedIndex::reduceFunc);
            } else {
                mr = Master.distributed("wcdis", files, 3, args[1]);
            }
            mr.mWait();
        } else {
            Worker.runWorker(args[1], args[2], InvertedIndex::mapFunc, InvertedIndex::reduceFunc, 100, null);
        }
    }
}
