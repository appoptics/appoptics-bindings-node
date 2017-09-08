require 'json'

#
# This Vagrantfile builds a dev box with all the parts needed for testing
#
$script = <<-BASH
sudo apt-get -y update
sudo apt-get -y install software-properties-common python-software-properties \
  build-essential curl git wget

sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get -qq update
sudo apt-get -qq install g++-4.8 gcc-4.8
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-4.8 20
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-4.8 20
sudo update-alternatives --config gcc
sudo update-alternatives --config g++

# tracelyzer
#wget https://files.tv.solarwinds.com/install_traceview.sh
#sudo sh ./install_traceview.sh f08da708-7f1c-4935-ae2e-122caf1ebe31
sudo sh /vagrant/appoptics/install-appoptics-daemon.sh f08da708-7f1c-4935-ae2e-122caf1ebe31

# [ao]abortive attempt to avoid install script but tracelyzer package requires liboboe.
#ACCESS_KEY=f08da708-7f1c-4935-ae2e-122caf1ebe31
#sudo echo "tracelyzer.access_key=$ACCESS_KEY" > /etc/tracelytics.conf
#sudo dpkg -i /appoptics/tracelyzer_1.3.6-xenial1_amd64.deb
#sudo apt-get install -f

# node/nvm
curl -o- https://raw.githubusercontent.com/creationix/nvm/v0.29.0/install.sh | bash
echo 'if [[ ":$PATH:" != *":node_modules/.bin:"* ]]; then PATH=${PATH}:node_modules/.bin; fi' >> $HOME/.bashrc
source $HOME/.nvm/nvm.sh
# [ao] not working with version 8
nvm install 4.7
nvm alias default 4.7
# [ao] nvm install stable
# [ao] nvm alias default stable
BASH

Vagrant.configure(2) do |config|
  config.vm.box = 'ubuntu/trusty64'

  # config.vm.network 'private_network', ip: '192.168.0.222'
  config.vm.synced_folder '.', '/vagrant', id: 'core'
    # nfs: true, nfs_udp: false,
    # mount_options: ['nolock,vers=3,noatime,actimeo=1']

  config.vm.provision 'shell', privileged: false, inline: $script

  # Virtualbox VM
  config.vm.provider :virtualbox do |provider|
    # Cap cpu and memory usage
    provider.customize [
      'modifyvm', :id,
      '--memory', 1024,
      '--cpuexecutioncap', 50
    ]

    # Enable symlink support
    provider.customize [
      'setextradata', :id,
      'VBoxInternal2/SharedFoldersEnableSymlinksCreate/v-root', '1'
    ]
  end
end