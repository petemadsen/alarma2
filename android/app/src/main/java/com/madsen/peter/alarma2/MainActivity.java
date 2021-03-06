package com.madsen.peter.alarma2;

import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.renderscript.ScriptGroup;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;

import android.bluetooth.BluetoothAdapter;
import android.text.InputType;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import com.madsen.peter.alarma2.bt.ArduinoCommands;
import com.madsen.peter.alarma2.bt.BTUtils;
import com.madsen.peter.alarma2.bt.Connection;

public class MainActivity extends AppCompatActivity {
    final Connection conn = new Connection();

    private static final String PREFS_NAME = "MyPrefsFile";
    private static final String CFG_BT_DEVICE = "bt_device";
    private static final String CFG_PASSWORD = "password";

    private String m_password = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        findViewById(R.id.btn_beep).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                action_beep();
            }
        });

        findViewById(R.id.btn_passwd).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                action_password();
            }
        });

        findViewById(R.id.btn_activate).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                action_activate(true);
            }
        });

        findViewById(R.id.btn_deactivate).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                action_activate(false);
            }
        });

        Button btn_select_bt = findViewById(R.id.btn_bt_device);
        btn_select_bt.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                action_select_bt();
            }
        });

        // Restore preferences
        SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);
        String bt_device = settings.getString(CFG_BT_DEVICE, "...");
        btn_select_bt.setText(bt_device);

        m_password = settings.getString(CFG_PASSWORD, "");
    }

    @Override
    protected void onStart() {
        super.onStart();

        final Button btn_select_bt = findViewById(R.id.btn_bt_device);
        if (btn_select_bt.getText() == "...")
            return;

        do_connect(btn_select_bt.getText().toString());
    }

    private void do_connect(final String device) {
        final EditText log = (EditText) findViewById(R.id.log);
        log.setText("Connecting to " + device + "...");

        final Button btn_activate = findViewById(R.id.btn_activate);
        final Button btn_deactivate = findViewById(R.id.btn_deactivate);
        final Button btn_password = findViewById(R.id.btn_passwd);

        btn_activate.setEnabled(false);
        btn_deactivate.setEnabled(false);
        btn_password.setEnabled(false);

        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    conn.disconnect();
                    conn.connect(device);
                    MainActivity.this.runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            log.setText("Connected to " + device);
                            btn_password.setEnabled(true);
                        }
                    });
                } catch (final Exception ex) {
                    MainActivity.this.runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            log.setText("Error" + ex.getMessage());
                        }
                    });
                } finally {
                }
            }
        }).start();
    }

    @Override
    public void onStop() {
        super.onStop();

        final Button btn_activate = findViewById(R.id.btn_activate);
        final Button btn_deactivate = findViewById(R.id.btn_deactivate);
        final Button btn_beep = findViewById(R.id.btn_beep);

        btn_activate.setEnabled(false);
        btn_deactivate.setEnabled(false);
        btn_beep.setEnabled(false);

        try {
            conn.disconnect();
        } catch (Exception ignored) {
        }


        save_settings();
    }

    private void save_settings() {
        final Button btn_select_bt = findViewById(R.id.btn_bt_device);

        SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);
        SharedPreferences.Editor editor = settings.edit();
        editor.putString(CFG_BT_DEVICE, btn_select_bt.getText().toString());
        editor.putString(CFG_PASSWORD, m_password);
        Log.d("xxx", btn_select_bt.getText().toString());

        editor.commit();
    }

    private void action_password() {
        final EditText taskEditText = new EditText(this);
        taskEditText.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_VARIATION_PASSWORD);

        AlertDialog.Builder alert = new AlertDialog.Builder(this);
        alert.setTitle("Password");
        alert.setView(taskEditText);
        alert.setPositiveButton("OK", new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialogInterface, int i) {
                m_password = String.valueOf(taskEditText.getText());

                findViewById(R.id.btn_activate).setEnabled(true);
                findViewById(R.id.btn_deactivate).setEnabled(true);
                findViewById(R.id.btn_beep).setEnabled(true);

                save_settings();
            }
        });
        alert.show();
    }

    private void action_beep() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    append_to_log(ArduinoCommands.CMD_BEEP);
                    final String reply = conn.send(ArduinoCommands.CMD_BEEP);
                    append_to_log(reply);
                    make_toast(R.string.beep);

                } catch (Exception ex) {
//                    append_to_log(R.string.);
//                    ex.printStackTrace();
//                    Log.e("", ffff);
                }
            }
        }).start();
    }

    private void action_activate(final boolean activate) {
        final String cmd = activate ? ArduinoCommands.CMD_ACTIVATE : ArduinoCommands.CMD_DEACTIVATE;
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    String str = cmd + m_password;
                    append_to_log(cmd);
                    String reply = conn.send(str);
                    append_to_log(reply);
                } catch(Exception ex) {
                }
            }
        }).start();
    }

    private void action_select_bt() {
        if (!BTUtils.isEnabled()) {
            Intent enableBluetooth = new Intent(
                    BluetoothAdapter.ACTION_REQUEST_ENABLE);
            startActivityForResult(enableBluetooth, 0);
            return;
        }

        final CharSequence[] choiceList = BTUtils.get_devices(this);
        if (choiceList == null) {
            return;
        }
//        Log.d(LOG_NAME, "###### " + choiceList.length);

        final Button btn_select_bt = (Button) findViewById(R.id.btn_bt_device);

        AlertDialog.Builder alert = new AlertDialog.Builder(this);
        alert.setTitle("BT");
        alert.setItems(choiceList, new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int which) {
                btn_select_bt.setText(choiceList[which]);
                do_connect(choiceList[which].toString());
            }
        });

        alert.show();
    }

    private void append_to_log(final String text) {
        MainActivity.this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                TextView log = findViewById(R.id.txt_log);
                log.append(text + "\n");
            }
        });
    }

    private void make_toast(final int string_id) {
        MainActivity.this.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(getApplicationContext(), R.string.beep, Toast.LENGTH_SHORT).show();
            }
        });
    }
}
