package com.injector.app

import android.os.Bundle
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var etPackageName: EditText
    private lateinit var etSoPath: EditText
    private lateinit var tvStatus: TextView
    private lateinit var btnInject: Button
    private lateinit var btnLaunch: Button
    private lateinit var btnRefresh: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        initViews()
        setupListeners()
        updateZygiskStatus()
    }

    private fun initViews() {
        etPackageName = findViewById(R.id.et_package_name)
        etSoPath = findViewById(R.id.et_so_path)
        tvStatus = findViewById(R.id.tv_status)
        btnInject = findViewById(R.id.btn_inject)
        btnLaunch = findViewById(R.id.btn_launch)
        btnRefresh = findViewById(R.id.btn_refresh)

        etSoPath.setText("/data/data/com.injector.app/lib/libtool.so")
    }

    private fun setupListeners() {
        btnInject.setOnClickListener {
            injectSo()
        }

        btnLaunch.setOnClickListener {
            launchApp()
        }

        btnRefresh.setOnClickListener {
            updateZygiskStatus()
        }
    }

    private fun injectSo() {
        val packageName = etPackageName.text.toString().trim()
        val soPath = etSoPath.text.toString().trim()

        when {
            packageName.isEmpty() -> {
                Toast.makeText(this, "请输入包名", Toast.LENGTH_SHORT).show()
            }
            soPath.isEmpty() -> {
                Toast.makeText(this, "请输入 SO 路径", Toast.LENGTH_SHORT).show()
            }
            !File(soPath).exists() -> {
                Toast.makeText(this, "SO 文件不存在", Toast.LENGTH_SHORT).show()
            }
            else -> {
                try {
                    val config = "$packageName:$soPath:onLoad"
                    val configFile = java.io.File("/data/adb/zygisk/injector_targets.txt")
                    configFile.parentFile?.mkdirs()
                    configFile.writeText(config)
                    Toast.makeText(this, "配置已写入，请重启目标应用", Toast.LENGTH_LONG).show()
                } catch (e: Exception) {
                    Toast.makeText(this, "写入失败: ${e.message}", Toast.LENGTH_SHORT).show()
                }
            }
        }
    }

    private fun launchApp() {
        val packageName = etPackageName.text.toString().trim()

        if (packageName.isEmpty()) {
            Toast.makeText(this, "请输入包名", Toast.LENGTH_SHORT).show()
            return
        }

        try {
            val intent = packageManager.getLaunchIntentForPackage(packageName)
            if (intent != null) {
                startActivity(intent)
            } else {
                Toast.makeText(this, "无法启动应用", Toast.LENGTH_SHORT).show()
            }
        } catch (e: Exception) {
            Toast.makeText(this, "启动失败: ${e.message}", Toast.LENGTH_SHORT).show()
        }
    }

    private fun updateZygiskStatus() {
        val zygiskDir = java.io.File("/data/adb/zygisk")
        val moduleFile = java.io.File("/data/adb/zygisk/Zygisk-Injector")
        val isConnected = zygiskDir.exists() || moduleFile.exists()

        if (isConnected) {
            tvStatus.text = "✅ Zygisk 已连接"
            tvStatus.setTextColor(android.graphics.Color.parseColor("#4CAF50"))
        } else {
            tvStatus.text = "❌ Zygisk 未连接"
            tvStatus.setTextColor(android.graphics.Color.parseColor("#F44336"))
        }
    }
}