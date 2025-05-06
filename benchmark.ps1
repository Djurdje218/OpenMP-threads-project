$program = ".\main.exe"  
$input = "test_data/225H_xc.ppm"
$output = "test_data/out.pnm"
$coef = "0.00390625"
$threadCounts = @(1, 2, 4, 8, 16) 
$outputFile = "output/output.txt"

# Clear previous output file
if (Test-Path $outputFile) { Clear-Content -Path $outputFile }

Write-Output "Running benchmark with different thread counts..." | Tee-Object -FilePath $outputFile -Append

foreach ($threads in $threadCounts) {
    Write-Output "=============================================" | Tee-Object -FilePath $outputFile -Append
    Write-Output "Testing with $threads threads..." | Tee-Object -FilePath $outputFile -Append
    Write-Output "=============================================" | Tee-Object -FilePath $outputFile -Append

    for ($i = 1; $i -le 100; $i++) {
        Write-Output "Run $i using $threads threads:" | Tee-Object -FilePath $outputFile -Append  
        & $program --input $input --output $output --coef $coef --omp-threads $threads | Tee-Object -FilePath $outputFile -Append
        Write-Output "--------------------------------------" | Tee-Object -FilePath $outputFile -Append  
    }
}

Write-Output "Benchmark completed. Results saved to $outputFile" | Tee-Object -FilePath $outputFile -Append
