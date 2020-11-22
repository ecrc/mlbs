pipeline {
    agent { label 'jenkinsfile' }
    triggers {
        pollSCM('H/10 * * * *')
    }
    options {
        disableConcurrentBuilds()
        buildDiscarder(logRotator(numToKeepStr: '50'))
        timestamps()
    }
    stages {
        stage ('build') {
            steps {
            sh '''#!/bin/bash -el
                    # The -x flags indicates to echo all commands, thus knowing exactly what is being executed.
                    # The -e flags indicates to halt on error, so no more processing of this script will be done
                    # if any command exits with value other than 0 (zero)
module purge 
module load gcc/5.5.0
make clean 
make 
'''
    }
           }               
        stage ('test') {
            steps {
            sh '''#!/bin/bash -el
./fakeRTM
'''
}
    }
        }
          }
