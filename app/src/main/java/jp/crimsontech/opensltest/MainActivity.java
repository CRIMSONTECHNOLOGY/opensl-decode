package jp.crimsontech.opensltest;

import android.content.res.AssetManager;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("native-lib");
    }

    RadioGroup group;
    CheckBox check;
    EditText edit;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        group = (RadioGroup) findViewById(R.id.radio_group_content_type);
        group.check(R.id.radio_mp3);

        check = (CheckBox)findViewById(R.id.check_seek);
        check.setChecked(true);

        edit = (EditText)findViewById(R.id.edit_text_out_path);
        String path = getApplicationContext().getExternalFilesDir(Environment.DIRECTORY_MUSIC) + "/decoded.wav";
        edit.setText(path);

        findViewById(R.id.button_decode).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                String fname;
                switch (group.getCheckedRadioButtonId()) {
                    case R.id.radio_ogg:
                        fname = "test.ogg";
                        break;
                    case R.id.radio_aac:
                        fname = "test.m4a";
                        break;
                    case R.id.radio_mp3:
                        fname = "test.mp3";
                        break;
                    case R.id.radio_wav:
                    default:
                        fname = "test.wav";
                        break;
                }
                toWavFile(getAssets(), fname, edit.getText().toString(), check.isChecked());
                Toast.makeText(MainActivity.this, "Successful", Toast.LENGTH_LONG).show();
            }
        });

        Button buttonPlay = (Button)findViewById(R.id.button_play);
        buttonPlay.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final MediaPlayer player = MediaPlayer.create(MainActivity.this, Uri.fromFile(new File(edit.getText().toString())));
                player.setOnCompletionListener(new MediaPlayer.OnCompletionListener() {
                    @Override
                    public void onCompletion(MediaPlayer mp) {
                        player.release();
                    }
                });

                player.setOnPreparedListener(new MediaPlayer.OnPreparedListener() {
                    @Override
                    public void onPrepared(MediaPlayer mp) {
                        mp.start();
                    }
                });

            }
        });

    }

    /**
     * native methods
     */
    public static native boolean toWavFile(AssetManager assetManager,
                                           String filename,
                                           String exportWavPath,
                                           boolean execSeek);

}
